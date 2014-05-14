#include <stdlib.h>
#include <assert.h>

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN

#include <windows.h>
#include "platform.h"

struct mr_thread {
	HANDLE handle;
	void* arg;
	unsigned int (*routine)(void* arg);
};

struct mr_mutex {
	HANDLE handle;
};

__stdcall DWORD mr_thread_wrapper(void* t)
{
	mr_thread* thread = (mr_thread*)t;
	return thread->routine(thread->arg);
}

mr_thread* mr_create_thread(unsigned int (*routine)(void* arg), void* arg)
{
	mr_thread* thread = calloc(1, sizeof(mr_thread));
	if(!thread)
		return NULL;

	thread->routine = routine;
	thread->arg = arg;

	thread->handle = CreateThread(NULL, 0, mr_thread_wrapper, thread, 0, NULL);

	return thread;
}

mr_mutex* mr_create_mutex()
{
	mr_mutex* mutex = calloc(1, sizeof(mr_mutex));
	if(!mutex)
		return NULL;
	
	mutex->handle = CreateMutex(NULL, FALSE, NULL);

	return mutex;
}

void mr_lock_mutex(mr_mutex* mutex)
{
	WaitForSingleObject(mutex->handle, INFINITE);
}

void mr_unlock_mutex(mr_mutex* mutex)
{
	ReleaseMutex(mutex->handle);
}

void mr_sleep(int ms)
{
	Sleep(ms);
}

unsigned int mr_wait_thread(mr_thread* thread)
{
	WaitForSingleObject(thread->handle, INFINITE);
	DWORD ret = 0;
	BOOL e = GetExitCodeThread(thread->handle, &ret);
	assert(e);
	return ret;
}
