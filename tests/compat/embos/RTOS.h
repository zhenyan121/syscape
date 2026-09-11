#ifndef RTOS_H
#define RTOS_H

#ifdef __cplusplus
extern "C" {
#endif

#define OS_VERSION 51801U
#define OS_VERSION_GENERIC 51801U
#define OS_SP_SMP 1
#define OS_CORE_NUM_CORES 4U
#define OS_TICK_FREQ 1000U
#define OS_TOTAL_HEAP_SIZE 65536U

typedef unsigned int OS_U32;
typedef unsigned int OS_UINT;
typedef unsigned char OS_U8;
typedef OS_U32 OS_TIME;
typedef OS_U8 OS_PRIO;
typedef struct OS_TASK_struct OS_TASK;

#ifndef OS_CONST_PTR
#define OS_CONST_PTR const
#endif

#ifndef OS_NULL
#define OS_NULL 0
#endif

OS_U32 OS_GetVersion(void);
OS_TIME OS_TIME_GetTicks(void);
OS_TIME OS_GetTime32(void);
OS_U32 OS_TIME_ConvertTicks2ms(OS_TIME Ticks);
OS_U32 OS_CORE_GetNumCores(void);
OS_PRIO OS_TASK_GetPriority(OS_CONST_PTR OS_TASK* pTask);
OS_PRIO OS_GetPriority(OS_CONST_PTR OS_TASK* pTask);
OS_UINT OS_TASK_GetNumTasks(void);
OS_U32 OS_GetFreeHeapSpace(void);

#ifdef __cplusplus
}
#endif

#endif
