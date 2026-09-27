#ifndef TESTS_COMPAT_RTTHREAD_RTTHREAD_H
#define TESTS_COMPAT_RTTHREAD_RTTHREAD_H

#include "rtdef.h"

#ifdef __cplusplus
extern "C" {
#endif

rt_tick_t rt_tick_get(void);
void rt_memory_info(rt_size_t* total, rt_size_t* used, rt_size_t* max_used);
rt_thread_t rt_thread_self(void);
rt_int32_t rt_object_get_length(rt_uint8_t type);

// Mock controller functions for test suites
void rtthread_mock_set_tick(rt_tick_t tick);
void rtthread_mock_set_memory(rt_size_t total, rt_size_t used,
                              rt_size_t max_used);
void rtthread_mock_set_thread(rt_thread_t thread);
void rtthread_mock_set_priority(rt_uint8_t priority);
void rtthread_mock_set_thread_count(rt_int32_t count);
void rtthread_mock_reset(void);

#ifdef __cplusplus
}
#endif

#endif
