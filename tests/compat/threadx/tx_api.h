#ifndef TX_API_H
#define TX_API_H

#include "tx_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define THREADX_MAJOR_VERSION 6
#define THREADX_MINOR_VERSION 4
#define THREADX_PATCH_VERSION 0

/* Standard ThreadX version string identifier */
extern const CHAR _tx_version_id[];

/* Kernel thread structure stub */
typedef struct TX_THREAD_STRUCT {
    ULONG tx_thread_id;
} TX_THREAD;

/* APIs */
ULONG tx_time_get(VOID);
TX_THREAD* tx_thread_identify(VOID);
UINT tx_thread_info_get(TX_THREAD* thread_ptr, CHAR** name, UINT* state,
                        ULONG* run_count, UINT* priority,
                        UINT* preemption_threshold, ULONG* time_slice,
                        TX_THREAD** next_thread, TX_THREAD** suspended_thread);

/* Thread counter in ThreadX kernel */
extern ULONG _tx_thread_created_count;

#ifdef __cplusplus
}
#endif

#endif
