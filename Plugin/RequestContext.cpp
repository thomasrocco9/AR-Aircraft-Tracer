#include "RequestContext.h"

#include "cJSON.h"

#include "Common.h"

bool request_context::Init()
{
	WorkQueue.Init();

	Internet = InternetOpen("Mozilla/5.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, INTERNET_FLAG_ASYNC);
	if (!Internet)
	{
		HandleConnectionLost();
		return false;
	}

	INTERNET_STATUS_CALLBACK Callback = InternetSetStatusCallback(Internet, InternetStatusCallback);
	if (Callback == INTERNET_INVALID_STATUS_CALLBACK)
	{
		HandleConnectionLost();
		return 0;
	}

	ReadBuf = (char*)malloc(READ_BUF_SIZE);

	const char* Username = "mlavelle";
	const char* Password = "XAircraft123";
	Connection = InternetConnect(Internet, "opensky-network.org", INTERNET_DEFAULT_HTTPS_PORT, Username, Password, INTERNET_SERVICE_HTTP, 0, (DWORD_PTR)this);
	if (!Connection)
	{
		HandleConnectionLost();
		return false;
	}

	HTTPFile = HttpOpenRequest(Connection, "GET", "/api/states/all", NULL, NULL, NULL, INTERNET_FLAG_SECURE, (DWORD_PTR)this);
	if (!HTTPFile)
	{
		HandleConnectionLost();
		return false;
	}

	SendRequest();

	PrevHashTable.Init();
	CurrHashTable.Init();

	Database = fopen("FilteredDatabase.bin", "rb");

	return true;
}

void request_context::Free()
{
	free(Hashes);
	CurrHashTable.Free();
	PrevHashTable.Free();

	FreeJSON();
	InternetCloseHandle(HTTPFile);
	InternetCloseHandle(Connection);
	InternetCloseHandle(Internet);

	fclose(Database);
}

void request_context::FreeJSON()
{
	if (JSONBuf)
	{
		free(JSONBuf);
		JSONBuf = NULL;
		JSONLength = 0;
	}
}

void request_context::FreeHashes()
{
	if (Hashes)
	{
		free(Hashes);
		Hashes = NULL;
		HashCount = 0;
	}
}

bool request_context::Update(float ElapsedTimeSinceLastCall)
{
	if (LostConnection)
	{
		return false;
	}

	Timer += ElapsedTimeSinceLastCall;

	while (State == REQUEST_CONTEXT_STATE_READ)
	{
		if (!InternetReadFile(HTTPFile, ReadBuf, READ_BUF_SIZE, &BytesRead))
		{
			int Error = GetLastError();
			if (Error == ERROR_IO_PENDING)
			{
				State = REQUEST_CONTEXT_STATE_READ_COMPLETED;
			}
			else
			{
				HandleConnectionLost();
				return false;
			}
		}
		else
		{
			UpdateJSON();
		}
	}

	static parse_json_work Work;

	if (State == REQUEST_CONTEXT_STATE_PARSE)
	{
		Work.Ctx = this;
		Work.HashTable.Init();

		State = REQUEST_CONTEXT_STATE_NONE;
		WorkQueue.PushWork(ParseJSON, &Work);
	}

	if (State == REQUEST_CONTEXT_STATE_COPY)
	{
		FreeHashes();

		HashCount = Work.HashCount;
		Hashes = (int*)malloc(HashCount * sizeof(int));
		memcpy(Hashes, Work.Hashes, HashCount * sizeof(int));

		CurrHashTable.CopyTo(PrevHashTable);
		Work.HashTable.CopyTo(CurrHashTable);

		Work.HashTable.Free();

		free(Work.Hashes);
		Work.Hashes = NULL;
		Work.HashCount = 0;

		FreeJSON();

		Timer = 0.0f;

		State = REQUEST_CONTEXT_STATE_SEND;
	}

	if (State == REQUEST_CONTEXT_STATE_SEND)
	{
		if (Timer >= SEND_TIME_THRESHOLD)
		{
			SendRequest();
		}
	}
}

void request_context::UpdateJSON()
{
	const char* TooManyRequests = "Too many requests";
	if (strncmp(ReadBuf, TooManyRequests, sizeof(TooManyRequests)) == 0)
	{
		HandleConnectionLost();
	}

	if (BytesRead == 0)
	{
		State = REQUEST_CONTEXT_STATE_PARSE;
	}
	else
	{
		if (JSONLength == 0)
		{
			JSONBuf = (char*)malloc(BytesRead);
		}
		else
		{
			JSONBuf = (char*)realloc(JSONBuf, JSONLength + BytesRead);
		}

		memcpy(JSONBuf + JSONLength, ReadBuf, BytesRead);
		JSONLength += BytesRead;
	}
}

void request_context::SendRequest()
{
	if (!HttpSendRequest(HTTPFile, NULL, 0, 0, 0))
	{
		if (GetLastError() == ERROR_IO_PENDING)
		{
			State = REQUEST_CONTEXT_STATE_SEND_COMPLETED;
		}
		else
		{
			return;
		}
	}
}

uint32_t ReadUInt32(FILE* File)
{
	uint32_t Result;
	fread(&Result, sizeof(Result), 1, File);
	Result = _byteswap_ulong(Result);
	return Result;
}

uint64_t ReadUInt64(FILE* File)
{
	uint64_t Result;
	fread(&Result, sizeof(Result), 1, File);
	Result = _byteswap_uint64(Result);
	return Result;
}

float ReadFloat(FILE* File)
{
	uint32_t Val = ReadUInt32(File);
	float Result = *((float*)(&Val));
	return Result;
}

double ReadDouble(FILE* File)
{
	uint64_t Val = ReadUInt64(File);
	double Result = *((double*)(&Val));
	return Result;
}

aircraft_state ReadAircraftState(FILE* File)
{
	aircraft_state Result = {};

	char ICAOHexString[7];
	fread(ICAOHexString, sizeof(ICAOHexString[0]), ArrayCount(ICAOHexString), File);

	Result.ICAO = strtol(ICAOHexString, NULL, 16);
	Result.Latitude = (float)ReadDouble(File);
	Result.Longitude = (float)ReadDouble(File);
	Result.Altitude = ReadFloat(File);
	Result.Velocity = ReadFloat(File);
	Result.Heading = ReadFloat(File);

	return Result;
}

void WriteUInt32(uint32_t Val, FILE* File)
{
	Val = _byteswap_ulong(Val);
	fwrite(&Val, sizeof(Val), 1, File);
}

void WriteUInt64(uint64_t Val, FILE* File)
{
	Val = _byteswap_uint64(Val);
	fwrite(&Val, sizeof(Val), 1, File);
}

void WriteFloat(float Val, FILE* File)
{
	uint32_t uVal = *((uint32_t*)(&Val));
	WriteUInt32(uVal, File);
}

void WriteDouble(double Val, FILE* File)
{
	uint64_t uVal = *((uint64_t*)(&Val));
	WriteUInt64(uVal, File);
}

void WriteAircraftState(aircraft_state State, FILE* File)
{
	char ICAOHexString[7];
	snprintf(ICAOHexString, sizeof(ICAOHexString), "%x", State.ICAO);
	fwrite(ICAOHexString, sizeof(ICAOHexString[0]), ArrayCount(ICAOHexString), File);

	WriteDouble(State.Latitude, File);
	WriteDouble(State.Longitude, File);
	WriteFloat(State.Altitude, File);
	WriteFloat(State.Velocity, File);
	WriteFloat(State.Heading, File);
}

void request_context::UpdateFromDatabase(float ElapsedTime)
{
	Timer = ElapsedTime - PrevElapsedTime;

	if (Timer > SEND_TIME_THRESHOLD)
	{
		fseek(Database, 0, SEEK_END);
		int FileSize = ftell(Database);
		fseek(Database, 0, SEEK_SET);

		uint32_t ElapsedTimestamp = (uint32_t)(ElapsedTime);

		while ((FileSize - ftell(Database)) > 0)
		{
			uint32_t Timestamp = (uint32_t)ReadUInt64(Database);
			uint32_t AircraftStateCount = ReadUInt32(Database);

			if (!StartTimestamp)
			{
				StartTimestamp = Timestamp;
			}

			if ((Timestamp - StartTimestamp) >= ElapsedTimestamp)
			{
				FreeHashes();

				HashCount = AircraftStateCount;
				Hashes = (int*)calloc(HashCount, sizeof(int));

				CurrHashTable.CopyTo(PrevHashTable);

				for (uint32_t i = 0; i < AircraftStateCount; ++i)
				{
					aircraft_state State = ReadAircraftState(Database);
					Hashes[i] = State.ICAO;
					CurrHashTable.SetState(State);
				}
				Timer = 0.0f;
				PrevElapsedTime = ElapsedTime;
				break;
			}
			else
			{
				// Skip entry
				int AircraftStateSize = 35;
				fseek(Database, AircraftStateCount * AircraftStateSize, SEEK_CUR);
			}
		}
	}
}

void request_context::HandleConnectionLost()
{
	LostConnection = true;
	PrevHashTable.Free();
	PrevHashTable.Init();
	CurrHashTable.Free();
	CurrHashTable.Init();
	Timer = 0.0f;
}

void InternetStatusCallback(HINTERNET Internet, DWORD_PTR UserData, DWORD Status, LPVOID StatusInfo, DWORD StatusInfoLength)
{
	request_context* Ctx = (request_context*)UserData;

	INTERNET_ASYNC_RESULT* AsyncResult = (INTERNET_ASYNC_RESULT*)StatusInfo;

	switch (Status)
	{
	case INTERNET_STATUS_REQUEST_COMPLETE:
		switch (Ctx->State)
		{
		case REQUEST_CONTEXT_STATE_READ_COMPLETED:
			if (!AsyncResult->dwResult)
			{
				DWORD Error = AsyncResult->dwError;
				Ctx->HandleConnectionLost();
				break;
			}

			Ctx->UpdateJSON();
			Ctx->State = REQUEST_CONTEXT_STATE_READ; // Keep reading and overwrite UpdateJSON() decided state

			break;

		case REQUEST_CONTEXT_STATE_SEND_COMPLETED:
			if (AsyncResult->dwResult)
			{
				Ctx->State = REQUEST_CONTEXT_STATE_READ;
			}
			else
			{
				DWORD Error = AsyncResult->dwError;
				Ctx->HandleConnectionLost();
			}
			break;

		default:
			break;
		}
		break;

	default:
		break;
	}
}

WorkQueueCallback(ParseJSON)
{
	parse_json_work* Work = (parse_json_work*)UserParams;

	cJSON* JSON = cJSON_Parse(Work->Ctx->JSONBuf);
	if (JSON)
	{
		cJSON* Timestamp = cJSON_GetObjectItemCaseSensitive(JSON, "time");
		cJSON* States = cJSON_GetObjectItemCaseSensitive(JSON, "states");

		int StateCount = cJSON_GetArraySize(States);

		Work->HashCount = StateCount;
		Work->Hashes = (int*)calloc(Work->HashCount, sizeof(int));

		int HashIdx = 0;

		cJSON* State;
		cJSON_ArrayForEach(State, States)
		{
			cJSON* ICAOHex = cJSON_GetArrayItem(State, 0);
			int ICAO = (int)strtol(ICAOHex->valuestring, NULL, 16);

			aircraft_state* state = Work->HashTable.GetState(ICAO);
			Work->Hashes[HashIdx++] = ICAO;

			cJSON* Longitude = cJSON_GetArrayItem(State, 5);
			state->Longitude = (float)Longitude->valuedouble;

			cJSON* Latitude = cJSON_GetArrayItem(State, 6);
			state->Latitude = (float)Latitude->valuedouble;

			cJSON* Altitude = cJSON_GetArrayItem(State, 7);
			if (cJSON_IsNull(Altitude))
			{
				state->Altitude = 5.0f;
			}
			else
			{
				state->Altitude = (float)Altitude->valuedouble;
			}

			cJSON* Velocity = cJSON_GetArrayItem(State, 9);
			state->Velocity = (float)Velocity->valuedouble;

			cJSON* Heading = cJSON_GetArrayItem(State, 10);
			state->Heading = (float)Heading->valuedouble;
		}
	}

	Work->Ctx->State = REQUEST_CONTEXT_STATE_COPY;
}