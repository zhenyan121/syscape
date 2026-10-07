#ifndef CMSIS_OS2_H
#define CMSIS_OS2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>

#ifndef osCMSIS
#if defined(osCMSIS_RTX)
#define osCMSIS 0x10002U
#else
#define osCMSIS 0x20000U
#endif
#endif

#if !defined(CMSIS_RTOS_GENERIC_TEST) && !defined(osCMSIS_RTX)
#ifndef osCMSIS_RTX5
#define osCMSIS_RTX5 1
#endif
#endif

#ifndef osRtxVersionKernel
#if defined(osCMSIS_RTX)
#define osRtxVersionKernel 40820000U
#else
#define osRtxVersionKernel 50050001U
#endif
#endif

#ifndef osRtxVersionAPI
#define osRtxVersionAPI 20001000U
#endif

typedef int32_t osStatus_t;
#define osOK 0
#define osError (-1)
#define osErrorTimeout (-2)
#define osErrorResource (-3)
#define osErrorParameter (-4)
#define osErrorNoMemory (-5)
#define osErrorISR (-6)

typedef struct {
    uint32_t api;
    uint32_t kernel;
} osVersion_t;

typedef enum {
    osPriorityNone = 0,
    osPriorityIdle = 1,
    osPriorityLow = 8,
    osPriorityLow1 = 9,
    osPriorityBelowNormal = 16,
    osPriorityNormal = 24,
    osPriorityAboveNormal = 32,
    osPriorityHigh = 40,
    osPriorityRealtime = 48,
    osPriorityISR = 56,
    osPriorityError = -1
} osPriority_t;

typedef enum {
    osKernelInactive = 0,
    osKernelReady = 1,
    osKernelRunning = 2,
    osKernelLocked = 3,
    osKernelSuspended = 4,
    osKernelError = -1
} osKernelState_t;

typedef void* osThreadId_t;

osStatus_t osKernelGetInfo(osVersion_t* version, char* id_buf,
                           uint32_t id_size);
osKernelState_t osKernelGetState(void);
uint32_t osKernelGetTickCount(void);
uint32_t osKernelGetTickFreq(void);
uint32_t osKernelGetSysTickCount(void);
uint32_t osKernelGetSysTickFreq(void);

osThreadId_t osThreadGetId(void);
osPriority_t osThreadGetPriority(osThreadId_t thread_id);
uint32_t osThreadGetCount(void);

// Test control hooks to simulate edge cases and configurable values
void syscape_test_set_cmsis_rtos_tick_count(uint32_t ticks);
void syscape_test_set_cmsis_rtos_tick_freq(uint32_t freq);
void syscape_test_set_cmsis_rtos_kernel_version(uint32_t kernel_ver,
                                                const char* id_str);
void syscape_test_set_cmsis_rtos_thread_priority(osPriority_t prio);
void syscape_test_set_cmsis_rtos_thread_count(uint32_t count);
void syscape_test_set_cmsis_rtos_kernel_info_status(osStatus_t status);
void syscape_test_reset_cmsis_rtos_mock(void);

#ifdef __cplusplus
}
#endif

#endif // CMSIS_OS2_H
