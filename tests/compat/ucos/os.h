#ifndef OS_H
#define OS_H

#ifdef __cplusplus
extern "C" {
#endif

#define OS_VERSION 30800U
#define OS_CFG_TICK_RATE_HZ 1000U
#define OS_CORE_NUM_CORES 4U
#define OS_TOTAL_HEAP_SIZE 65536U
#define OS_CFG_SMP_EN 1U
#define LIB_DEF_MAX_CPU 4U

typedef unsigned int CPU_INT32U;
typedef unsigned short CPU_INT16U;
typedef unsigned char CPU_INT08U;
typedef unsigned int OS_TICK;
typedef unsigned int OS_ERR;
typedef CPU_INT16U OS_PRIO;
typedef unsigned int OS_OBJ_QTY;

typedef struct os_tcb {
    OS_PRIO Prio;
    CPU_INT08U OSTCBPrio;
} OS_TCB;

extern OS_TCB* OSTCBCurPtr;
extern OS_TCB* OSTCBCur;
extern OS_OBJ_QTY OSTaskQty;
extern CPU_INT08U OSTaskCtr;

CPU_INT16U OSVersion(OS_ERR* p_err);
OS_TICK OSTimeGet(OS_ERR* p_err);
CPU_INT32U OS_GetFreeHeapSpace(void);

#ifdef __cplusplus
}
#endif

#endif
