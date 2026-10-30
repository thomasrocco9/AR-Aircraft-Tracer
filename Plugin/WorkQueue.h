#pragma once

#include <Windows.h>

#include <stdint.h>

#define WorkQueueCallback(Name) void (Name)(void *UserParams, int ThreadIdx)
typedef WorkQueueCallback(*work_queue_callback);

struct work_queue_entry 
{
	work_queue_callback Callback;
	void*               UserParams;
};

// Single producer multiple consumer
class work_queue 
{
public:
	void Init();
	bool DoWork(int ThreadIdx);
	void PushWork(work_queue_callback Callback, void* UserParams); // Should only be called from main thread
	void WorkUntilDone(int ThreadIdx);

	HANDLE Semaphore;

private:
	volatile uint32_t Front, Back;
	volatile uint32_t EntryCount;
	work_queue_entry  Entries[256];
};

struct thread_info
{
	int         Idx;
	work_queue* Queue;
};

DWORD WINAPI ThreadProc(LPVOID Param);