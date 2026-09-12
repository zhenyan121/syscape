#include "ch.h"

namespace {

systime_t s_mock_systime = 42000;
tprio_t s_mock_prio = 64;
size_t s_mock_total_free = 32768;
size_t s_mock_largest_free = 16384;

thread_t s_mock_threads[3] = {{&s_mock_threads[1], "main", 64},
                              {&s_mock_threads[2], "worker", 128},
                              {nullptr, "idle", 1}};

} // namespace

extern "C" {

systime_t chVTGetSystemTimeX(void) {
    return s_mock_systime;
}

systime_t chVTGetSystemTime(void) {
    return s_mock_systime;
}

tprio_t chThdGetPriorityX(void) {
    return s_mock_prio;
}

thread_t* chThdGetSelfX(void) {
    return &s_mock_threads[0];
}

size_t chHeapStatus(memory_heap_t* heapp, size_t* totalp, size_t* largestp) {
    (void)heapp;
    if (totalp != nullptr) {
        *totalp = s_mock_total_free;
    }
    if (largestp != nullptr) {
        *largestp = s_mock_largest_free;
    }
    return 1;
}

thread_t* chRegFirstThread(void) {
    return &s_mock_threads[0];
}

thread_t* chRegNextThread(thread_t* tp) {
    if (tp != nullptr) {
        return tp->next;
    }
    return nullptr;
}

} // extern "C"
