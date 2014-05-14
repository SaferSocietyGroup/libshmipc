#ifndef PLATFORM_H
#define PLATFORM_H

typedef struct mr_thread mr_thread;
typedef struct mr_mutex mr_mutex;

mr_mutex* mr_create_mutex();
void mr_lock_mutex(mr_mutex* mutex);
void mr_unlock_mutex(mr_mutex* mutex);
void mr_sleep(int ms);

mr_thread* mr_create_thread(unsigned int (*routine)(void* arg), void* arg);
unsigned int mr_wait_thread(mr_thread* thread);

#endif
