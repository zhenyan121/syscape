#ifndef MBED_H
#define MBED_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef MBED_NO_VERSION_MACRO
#define MBED_VERSION_STRING "6.15.0"
#define MBED_MAJOR_VERSION 6
#define MBED_MINOR_VERSION 15
#define MBED_PATCH_VERSION 0
#define MBED_VERSION 61500
#endif

#ifndef MBED_NO_TICK_FREQ
#if defined(MBED_USE_TICK_FREQ_MACRO)
#ifndef MBED_CONF_RTOS_TICK_FREQ
#define MBED_CONF_RTOS_TICK_FREQ 1000U
#endif
#endif
#endif

#ifndef MBED_CPU_COUNT
#define MBED_CPU_COUNT 1U
#endif

#ifndef MBED_TOTAL_HEAP_SIZE
#define MBED_TOTAL_HEAP_SIZE 65536U
#endif

#ifndef MBED_HEAP_SIZE
#define MBED_HEAP_SIZE 65536U
#endif

#ifndef MBED_CONF_TARGET_DEFAULT_HEAP_SIZE
#define MBED_CONF_TARGET_DEFAULT_HEAP_SIZE 65536U
#endif

typedef uint32_t osStatus_t;
typedef void* osThreadId_t;

typedef enum {
    osPriorityNone = 0,
    osPriorityIdle = 1,
    osPriorityLow = 8,
    osPriorityNormal = 24,
    osPriorityAboveNormal = 32,
    osPriorityHigh = 40,
    osPriorityRealtime = 48,
    osPriorityError = -1
} osPriority_t;

typedef struct {
    uint32_t current_size;
    uint32_t max_size;
    uint32_t total_size;
    uint32_t reserved_size;
    uint32_t alloc_cnt;
    uint32_t alloc_fail_cnt;
} mbed_stats_heap_t;

uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickFreq(void);
osThreadId_t osThreadGetId(void);
osPriority_t osThreadGetPriority(osThreadId_t thread_id);
uint32_t osThreadGetCount(void);
void mbed_stats_heap_get(mbed_stats_heap_t* stats);

void mbed_mock_set_tick_count(uint32_t ticks);
void mbed_mock_set_tick_freq(uint32_t freq);
void mbed_mock_set_priority(osPriority_t prio);
void mbed_mock_set_thread_count(uint32_t count);
void mbed_mock_set_heap_stats(uint32_t current_size, uint32_t reserved_size);

#ifdef __cplusplus
}
#endif

#endif // MBED_H
