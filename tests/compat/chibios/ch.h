#ifndef CH_H
#define CH_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef CH_NO_VERSION_MACRO
#define CH_KERNEL_VERSION "8.0.1"
#define CH_KERNEL_MAJOR 8
#define CH_KERNEL_MINOR 0
#define CH_KERNEL_PATCH 1
#endif

#ifndef CH_CFG_ST_FREQUENCY
#define CH_CFG_ST_FREQUENCY 1000U
#endif

#ifndef PORT_CORES_NUMBER
#define PORT_CORES_NUMBER 4U
#endif

#ifndef CH_TOTAL_HEAP_SIZE
#define CH_TOTAL_HEAP_SIZE 65536U
#endif

#ifndef CH_CFG_USE_REGISTRY
#define CH_CFG_USE_REGISTRY 1
#endif

typedef uint32_t systime_t;
typedef uint32_t sysinterval_t;
typedef uint32_t tprio_t;

typedef struct ch_thread {
    struct ch_thread* next;
    const char* name;
    tprio_t prio;
} thread_t;

typedef struct ch_memory_heap {
    void* dummy;
} memory_heap_t;

systime_t chVTGetSystemTimeX(void);
systime_t chVTGetSystemTime(void);
tprio_t chThdGetPriorityX(void);
thread_t* chThdGetSelfX(void);

size_t chHeapStatus(memory_heap_t* heapp, size_t* totalp, size_t* largestp);

thread_t* chRegFirstThread(void);
thread_t* chRegNextThread(thread_t* tp);

#ifdef __cplusplus
}
#endif

#endif
