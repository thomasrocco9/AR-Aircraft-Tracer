#include "WorkQueue.h"

#include "Common.h"

static thread_info ThreadInfos[3];

void work_queue::Init()
{
	int ThreadCount = ArrayCount(ThreadInfos);

	Semaphore = CreateSemaphore(NULL, 0, ThreadCount, NULL);

	for (int i = 0; i < ThreadCount; ++i)
	{
		thread_info* Info = ThreadInfos + i;
		Info->Idx         = i + 1;
		Info->Queue       = this;

		DWORD ID;
		HANDLE Handle = CreateThread(NULL, 0, ThreadProc, Info, 0, &ID);
		CloseHandle(Handle);
	}
}

bool work_queue::DoWork(int ThreadIdx)
{
	bool Result = false;

	int ExpectedFront = Front;
	int NewFront      = (Front + 1) % ArrayCount(Entries);
	if (ExpectedFront != Back)
	{
		int ExchangedFront = InterlockedCompareExchange(&Front, NewFront, ExpectedFront);
		if (ExchangedFront == ExpectedFront)
		{
			work_queue_entry* Entry = Entries + ExchangedFront;
			Entry->Callback(Entry->UserParams, ThreadIdx);
			InterlockedDecrement(&EntryCount);
			Result = true;
		}
	}

	return Result;
}

void work_queue::PushWork(work_queue_callback Callback, void* UserParams)
{
	if (EntryCount < ArrayCount(Entries))
	{
		work_queue_entry* Entry = Entries + Back;
		Entry->Callback         = Callback;
		Entry->UserParams       = UserParams;

		Back = (Back + 1) % ArrayCount(Entries);

		InterlockedIncrement(&EntryCount);

		LONG PrevCount;
		ReleaseSemaphore(Semaphore, 1, &PrevCount);
	}
}

void work_queue::WorkUntilDone(int ThreadIdx)
{
	while (EntryCount > 0)
	{
		DoWork(ThreadIdx);
	}
}

DWORD WINAPI ThreadProc(LPVOID Param)
{
	thread_info* Info = (thread_info*)Param;

	DWORD ID = GetCurrentThreadId();

	for (;;)
	{
		if (!Info->Queue->DoWork(Info->Idx))
		{
			WaitForSingleObject(Info->Queue->Semaphore, INFINITE); // Put thread to sleep and wait for signal to wake up
		}
	}

	return 0;
}
