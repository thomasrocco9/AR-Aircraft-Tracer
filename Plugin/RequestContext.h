#pragma once

#include <stdio.h>

#include <Windows.h>
#include <wininet.h>

#include "AircraftState.h"
#include "WorkQueue.h"

#define SEND_TIME_THRESHOLD 20.0f
#define READ_BUF_SIZE       BUFSIZ

enum request_context_state
{
	REQUEST_CONTEXT_STATE_NONE,
	REQUEST_CONTEXT_STATE_SEND,
	REQUEST_CONTEXT_STATE_SEND_COMPLETED,
	REQUEST_CONTEXT_STATE_READ,
	REQUEST_CONTEXT_STATE_READ_COMPLETED,
	REQUEST_CONTEXT_STATE_PARSE,
	REQUEST_CONTEXT_STATE_COPY,
};

// State information involved with interacting with OpenSky-Network
class request_context
{
public:
	bool Init();
	void Free();
	void FreeJSON();
	void FreeHashes();
	bool Update(float ElapsedTimeSinceLastCall);
	void UpdateJSON();
	void SendRequest();

	void UpdateFromDatabase(float ElapsedTime);

	void HandleConnectionLost();

	int  HashCount;
	int* Hashes;

	aircraft_state_hash_table PrevHashTable;
	aircraft_state_hash_table CurrHashTable;

	float Timer;

	request_context_state State;

	int   JSONLength;
	char* JSONBuf;

	bool LostConnection;

private:
	float PrevElapsedTime;
	uint32_t StartTimestamp = 0;

	FILE* Database;

	work_queue WorkQueue;

	HINTERNET Internet;
	HINTERNET Connection;

	HINTERNET             HTTPFile;

	DWORD BytesRead;
	char* ReadBuf;
};

void InternetStatusCallback(HINTERNET Internet, DWORD_PTR UserData, DWORD Status, LPVOID StatusInfo, DWORD StatusInfoLength);

struct parse_json_work
{
	request_context* Ctx;
	int                       HashCount;
	int* Hashes;
	aircraft_state_hash_table HashTable;
};

WorkQueueCallback(ParseJSON);