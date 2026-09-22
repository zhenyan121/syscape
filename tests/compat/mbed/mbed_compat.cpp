#include "mbed.h"

static uint32_t g_mbed_tick_count = 5000U;
static uint32_t g_mbed_tick_freq = 1000U;
static osPriority_t g_mbed_priority = osPriorityNormal;
static uint32_t g_mbed_thread_count = 4U;
static mbed_stats_heap_t g_mbed_heap_stats = {
    16384U, // current_size
    32768U, // max_size
    65536U, // total_size
    65536U, // reserved_size
    10U,    // alloc_cnt
    0U      // alloc_fail_cnt
};

extern "C" {

uint32_t osKernelGetTickCount(void) {
    return g_mbed_tick_count;
}

uint32_t osKernelGetTickFreq(void) {
    return g_mbed_tick_freq;
}

osThreadId_t osThreadGetId(void) {
    static int dummy_thread = 1;
    return reinterpret_cast<osThreadId_t>(&dummy_thread);
}

osPriority_t osThreadGetPriority(osThreadId_t thread_id) {
    (void)thread_id;
    return g_mbed_priority;
}

uint32_t osThreadGetCount(void) {
    return g_mbed_thread_count;
}

void mbed_stats_heap_get(mbed_stats_heap_t* stats) {
    if (stats != nullptr) {
        *stats = g_mbed_heap_stats;
    }
}

void mbed_mock_set_tick_count(uint32_t ticks) {
    g_mbed_tick_count = ticks;
}

void mbed_mock_set_tick_freq(uint32_t freq) {
    g_mbed_tick_freq = freq;
}

void mbed_mock_set_priority(osPriority_t prio) {
    g_mbed_priority = prio;
}

void mbed_mock_set_thread_count(uint32_t count) {
    g_mbed_thread_count = count;
}

void mbed_mock_set_heap_stats(uint32_t current_size, uint32_t reserved_size) {
    g_mbed_heap_stats.current_size = current_size;
    g_mbed_heap_stats.reserved_size = reserved_size;
}

} // extern "C"
