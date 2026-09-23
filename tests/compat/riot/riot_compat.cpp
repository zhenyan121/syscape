#include "kernel_defines.h"
#include "riot_version.h"
#include "ztimer.h"
#include "ztimer64.h"
#include "xtimer.h"
#include "thread.h"
#include "cpu.h"
#include "malloc_monitor.h"

struct ztimer_clock {
    int dummy;
};

struct ztimer64_clock {
    int dummy;
};

static uint64_t g_mock_time_usec = 12345678000ULL;
static kernel_pid_t g_mock_current_pid = 1;
static thread_t g_mock_idle_thread = {0, 15, 0, "idle"};
static thread_t g_mock_active_thread = {1, 5, 0, "main"};

volatile int sched_num_threads = 4;
volatile thread_t* sched_active_thread = &g_mock_active_thread;
volatile thread_t* sched_threads[KERNEL_PID_LAST + 1] = {
    &g_mock_idle_thread,
    &g_mock_active_thread,
};

static size_t g_mock_ram_size = 65536;
static size_t g_mock_available_memory = 32768;

static ztimer_clock dummy_clock_msec = {1};
static ztimer_clock dummy_clock_usec = {2};
static ztimer_clock dummy_clock_sec = {3};

ztimer_clock_t* const _ztimer_msec = &dummy_clock_msec;
ztimer_clock_t* const _ztimer_usec = &dummy_clock_usec;
ztimer_clock_t* const _ztimer_sec = &dummy_clock_sec;

static ztimer64_clock dummy_clock64_msec = {1};
static ztimer64_clock dummy_clock64_usec = {2};
static ztimer64_clock dummy_clock64_sec = {3};

ztimer64_clock_t* const _ztimer64_msec = &dummy_clock64_msec;
ztimer64_clock_t* const _ztimer64_usec = &dummy_clock64_usec;
ztimer64_clock_t* const _ztimer64_sec = &dummy_clock64_sec;

uint32_t ztimer_now(ztimer_clock_t* clock) {
    (void)clock;
    return static_cast<uint32_t>((g_mock_time_usec / 1000ULL) & 0xFFFFFFFFULL);
}

uint64_t ztimer64_now(ztimer64_clock_t* clock) {
    (void)clock;
    return g_mock_time_usec / 1000ULL;
}

uint32_t xtimer_now_usec(void) {
    return static_cast<uint32_t>(g_mock_time_usec & 0xFFFFFFFFULL);
}

uint64_t xtimer_now_usec64(void) {
    return g_mock_time_usec;
}

kernel_pid_t thread_getpid(void) {
    return g_mock_current_pid;
}

thread_t* thread_get(kernel_pid_t pid) {
    if (pid >= 0 && pid <= KERNEL_PID_LAST) {
        // C-style cast explicitly acknowledging volatile stripping, matching
        // RIOT kernel's (thread_t *)sched_threads[pid] in
        // core/include/thread.h.
        return (thread_t*)sched_threads[pid];
    }
    return nullptr;
}

uint8_t thread_get_priority(const thread_t* thread) {
    if (thread != nullptr) {
        return thread->priority;
    }
    return 0;
}

void riot_mock_set_time_ms(uint32_t ms) {
    g_mock_time_usec = static_cast<uint64_t>(ms) * 1000ULL;
}

void riot_mock_set_time64_ms(uint64_t ms) {
    g_mock_time_usec = ms * 1000ULL;
}

void riot_mock_set_xtimer_usec(uint64_t usec) {
    g_mock_time_usec = usec;
}

void riot_mock_set_thread_count(int count) {
    sched_num_threads = count;
}

void riot_mock_set_active_thread(thread_t* thread) {
    sched_active_thread = thread;
}

void riot_mock_set_priority(uint8_t priority) {
    g_mock_active_thread.priority = priority;
}

void riot_mock_set_current_pid(kernel_pid_t pid) {
    g_mock_current_pid = pid;
}

void riot_mock_reset_threads(void) {
    for (int i = 0; i <= KERNEL_PID_LAST; ++i) {
        sched_threads[i] = nullptr;
    }
    sched_threads[0] = &g_mock_idle_thread;
    sched_threads[1] = &g_mock_active_thread;
    sched_num_threads = 4;
    sched_active_thread = &g_mock_active_thread;
    g_mock_current_pid = 1;
    g_mock_active_thread.priority = 5;
    g_mock_active_thread.status = 0;
    g_mock_active_thread.name = "main";
}

size_t cpu_get_ram_size(void) {
    return g_mock_ram_size;
}

void riot_mock_set_ram_size(size_t bytes) {
    g_mock_ram_size = bytes;
}

size_t get_mem_usage(void) {
    return g_mock_available_memory;
}

void riot_mock_set_available_memory(size_t bytes) {
    g_mock_available_memory = bytes;
}
