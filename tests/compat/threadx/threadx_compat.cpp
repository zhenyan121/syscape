#include "tx_api.h"

namespace {

TX_THREAD s_mock_thread = {0x54485244};
ULONG s_mock_time = 123456;
UINT s_mock_priority = 10;
CHAR s_mock_thread_name[] = "syscape_task";

} // namespace

extern "C" {

const CHAR _tx_version_id[] = "Eclipse ThreadX Version 6.4.0";
ULONG _tx_thread_created_count = 6;

ULONG tx_time_get(VOID) {
    return s_mock_time;
}

TX_THREAD* tx_thread_identify(VOID) {
    return &s_mock_thread;
}

UINT tx_thread_info_get(TX_THREAD* thread_ptr, CHAR** name, UINT* state,
                        ULONG* run_count, UINT* priority,
                        UINT* preemption_threshold, ULONG* time_slice,
                        TX_THREAD** next_thread, TX_THREAD** suspended_thread) {
    (void)thread_ptr;
    if (name != TX_NULL) {
        *name = s_mock_thread_name;
    }
    if (state != TX_NULL) {
        *state = 0;
    }
    if (run_count != TX_NULL) {
        *run_count = 42;
    }
    if (priority != TX_NULL) {
        *priority = s_mock_priority;
    }
    if (preemption_threshold != TX_NULL) {
        *preemption_threshold = s_mock_priority;
    }
    if (time_slice != TX_NULL) {
        *time_slice = 0;
    }
    if (next_thread != TX_NULL) {
        *next_thread = TX_NULL;
    }
    if (suspended_thread != TX_NULL) {
        *suspended_thread = TX_NULL;
    }
    return TX_SUCCESS;
}

} // extern "C"
