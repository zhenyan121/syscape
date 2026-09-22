#include "kernel_defines.h"
#include "riot_version.h"
#include "ztimer.h"
#include "ztimer64.h"
#include "xtimer.h"
#include "thread.h"

struct ztimer_clock {
    int dummy;
};

struct ztimer64_clock {
    int dummy;
};

static uint64_t g_mock_time64_ms = 12345678ULL;
static uint64_t g_mock_xtimer_usec = 12345678000ULL;
static kernel_pid_t g_mock_current_pid = 1;
static thread_t g_mock_active_thread = {1, 5, 0, "main"};
static thread_t* g_mock_threads[KERNEL_PID_LAST + 1] = {nullptr};

volatile int sched_num_threads = 4;
volatile thread_t* sched_active_thread = &g_mock_active_thread;
volatile thread_t* sched_threads[KERNEL_PID_LAST + 1] = {nullptr};

static void init_mock_threads() {
    static bool initialized = false;
    if (!initialized) {
        g_mock_threads[1] = &g_mock_active_thread;
        sched_threads[1] = &g_mock_active_thread;
        initialized = true;
    }
}

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
    return static_cast<uint32_t>(g_mock_time64_ms);
}

uint64_t ztimer64_now(ztimer64_clock_t* clock) {
    (void)clock;
    return g_mock_time64_ms;
}

uint32_t xtimer_now_usec(void) {
    return static_cast<uint32_t>(g_mock_xtimer_usec);
}

uint64_t xtimer_now_usec64(void) {
    return g_mock_xtimer_usec;
}

kernel_pid_t thread_getpid(void) {
    return g_mock_current_pid;
}

thread_t* thread_get(kernel_pid_t pid) {
    init_mock_threads();
    if (pid >= 0 && pid <= KERNEL_PID_LAST) {
        return g_mock_threads[pid];
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
    g_mock_time64_ms = static_cast<uint64_t>(ms);
    g_mock_xtimer_usec = static_cast<uint64_t>(ms) * 1000ULL;
}

void riot_mock_set_time64_ms(uint64_t ms) {
    g_mock_time64_ms = ms;
    g_mock_xtimer_usec = ms * 1000ULL;
}

void riot_mock_set_xtimer_usec(uint64_t usec) {
    g_mock_xtimer_usec = usec;
    g_mock_time64_ms = usec / 1000ULL;
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
