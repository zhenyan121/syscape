#ifndef THREAD_H
#define THREAD_H

#include "kernel_defines.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int16_t kernel_pid_t;
#define KERNEL_PID_UNDEF (-1)
#define KERNEL_PID_FIRST (1)
#define KERNEL_PID_LAST (32)

typedef struct _thread {
    kernel_pid_t pid;
    uint8_t priority;
    uint8_t status;
    const char* name;
} thread_t;

extern volatile int sched_num_threads;
extern volatile thread_t* sched_active_thread;
extern volatile thread_t* sched_threads[KERNEL_PID_LAST + 1];

kernel_pid_t thread_getpid(void);
thread_t* thread_get(kernel_pid_t pid);
uint8_t thread_get_priority(const thread_t* thread);

void riot_mock_set_thread_count(int count);
void riot_mock_set_active_thread(thread_t* thread);
void riot_mock_set_priority(uint8_t priority);
void riot_mock_set_current_pid(kernel_pid_t pid);

#ifdef __cplusplus
}
#endif

#endif // THREAD_H
