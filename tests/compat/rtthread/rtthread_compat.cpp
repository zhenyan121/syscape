#include "rtthread.h"

static rt_tick_t g_mock_tick = 12345678;
static rt_size_t g_mock_mem_total = 65536;
static rt_size_t g_mock_mem_used = 16384;
static rt_size_t g_mock_mem_max_used = 20480;

static struct rt_thread g_default_thread = {12, 12, "main"};
static rt_thread_t g_mock_thread = &g_default_thread;
static rt_int32_t g_mock_thread_count = 3;

extern "C" {

rt_tick_t rt_tick_get(void) {
    return g_mock_tick;
}

void rt_memory_info(rt_size_t* total, rt_size_t* used, rt_size_t* max_used) {
    if (total != nullptr) {
        *total = g_mock_mem_total;
    }
    if (used != nullptr) {
        *used = g_mock_mem_used;
    }
    if (max_used != nullptr) {
        *max_used = g_mock_mem_max_used;
    }
}

rt_thread_t rt_thread_self(void) {
    return g_mock_thread;
}

int rt_object_get_length(enum rt_object_class_type type) {
    if (type == RT_Object_Class_Thread) {
        return g_mock_thread_count;
    }
    return 0;
}

void rtthread_mock_set_tick(rt_tick_t tick) {
    g_mock_tick = tick;
}

void rtthread_mock_set_memory(rt_size_t total, rt_size_t used,
                              rt_size_t max_used) {
    g_mock_mem_total = total;
    g_mock_mem_used = used;
    g_mock_mem_max_used = max_used;
}

void rtthread_mock_set_thread(rt_thread_t thread) {
    g_mock_thread = thread;
}

void rtthread_mock_set_priority(rt_uint8_t priority) {
    g_default_thread.current_priority = priority;
}

void rtthread_mock_set_thread_count(rt_int32_t count) {
    g_mock_thread_count = count;
}

void rtthread_mock_reset(void) {
    g_mock_tick = 12345678;
    g_mock_mem_total = 65536;
    g_mock_mem_used = 16384;
    g_mock_mem_max_used = 20480;
    g_default_thread.current_priority = 12;
    g_default_thread.init_priority = 12;
    g_default_thread.name = "main";
    g_mock_thread = &g_default_thread;
    g_mock_thread_count = 3;
}

} // extern "C"
