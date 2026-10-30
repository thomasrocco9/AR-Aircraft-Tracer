#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

// MSVC exclusive used for byteswap instrinsic functions
#include <intrin.h>

#include <map>
#include <unordered_map>
#include <vector>

#define ArrayCount(Arr) (sizeof(Arr) / sizeof((Arr)[0]))

struct aircraft_state
{
	uint32_t ICAO;
	float    Latitude;
	float    Longitude;
	float    Altitude;
	float    Velocity;
	float    Heading;
};

struct aircraft_sample
{
	uint32_t       Timestamp;
	aircraft_state State;
};

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

	Result.ICAO      = strtol(ICAOHexString, NULL, 16);
	Result.Latitude  = (float)ReadDouble(File);
	Result.Longitude = (float) ReadDouble(File);
	Result.Altitude  = ReadFloat(File);
	Result.Velocity  = ReadFloat(File);
	Result.Heading   = ReadFloat(File);

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

bool IsInOregon(std::vector<aircraft_sample> FlightPath)
{
	bool Result = false;

	float MinLong =  41.99161889477894f;
	float MinLat  = -124.55417836738366f;
	float MaxLong =  46.26801803457034f;
	float MaxLat  = -116.46390970729706f;

	for (auto Sample : FlightPath)
	{
		float Lat = Sample.State.Latitude;
		float Long = Sample.State.Longitude;

		if (Lat > MinLat && Lat < MaxLat && Long > MinLong && Long < MaxLong)
		{
			Result = true;
			break;
		}
	}

	return Result;
}

int main(int Argc, char** Argv)
{
	FILE* Database = fopen("offlineDB.bin", "rb");

	fseek(Database, 0, SEEK_END);
	int FileSize = ftell(Database);
	fseek(Database, 0, SEEK_SET);

	// Key -> ICAO
	std::unordered_map<uint32_t, std::vector<aircraft_sample>> FlightPaths;

	while ((FileSize - ftell(Database)) > 0)
	{
		uint32_t Timestamp          = (uint32_t)ReadUInt64(Database);
		uint32_t AircraftStateCount = ReadUInt32(Database);

		for (uint32_t i = 0; i < AircraftStateCount; ++i)
		{
			aircraft_sample Sample = {};
			Sample.Timestamp       = Timestamp;
			Sample.State           = ReadAircraftState(Database);
			FlightPaths[Sample.State.ICAO].push_back(Sample);
		}
	}

	fclose(Database);

	for (auto Iterator = FlightPaths.begin(); Iterator != FlightPaths.end();)
	{
		auto FlightPath = Iterator->second;
		if (FlightPath.size() < 5000 || !IsInOregon(FlightPath))
		{
			Iterator = FlightPaths.erase(Iterator);
		}
		else
		{
			++Iterator;
		}
	}

	// Key -> Timestamp
	std::map<uint32_t, std::vector<aircraft_state>> Samples;

	for (auto Iterator : FlightPaths)
	{
		auto FlightPath = Iterator.second;
		for (auto Sample : FlightPath)
		{
			Samples[Sample.Timestamp].push_back(Sample.State);
		}
	}

	FILE* FilteredDatabase = fopen("FilteredDatabase.bin", "wb");

	for (auto Iterator : Samples)
	{
		uint32_t Timestamp = Iterator.first;
		auto     States    = Iterator.second;

		WriteUInt64(Timestamp, FilteredDatabase);
		WriteUInt32((uint32_t)States.size(), FilteredDatabase);

		for (auto State : States)
		{
			WriteAircraftState(State, FilteredDatabase);
		}
	}

	fclose(FilteredDatabase);

	return 0;
}
