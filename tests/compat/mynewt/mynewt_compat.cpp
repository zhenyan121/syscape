#include "os/os.h"

namespace {

os_time_t s_mock_time = 50000;
struct os_task s_mock_task = {nullptr, "main", 10, 1, 1};
size_t s_mock_free_heap = 32768;
uint32_t s_mock_task_count = 3;

} // namespace

extern "C" {

os_time_t os_time_get(void) {
    return s_mock_time;
}

struct os_task* os_sched_get_current_task(void) {
    return &s_mock_task;
}

uint32_t os_task_count(void) {
    return s_mock_task_count;
}

size_t os_get_free_heap_size(void) {
    return s_mock_free_heap;
}

void mynewt_mock_set_time(os_time_t t) {
    s_mock_time = t;
}

void mynewt_mock_set_task_prio(uint8_t prio) {
    s_mock_task.t_prio = prio;
}

void mynewt_mock_set_free_heap(size_t free_bytes) {
    s_mock_free_heap = free_bytes;
}

void mynewt_mock_set_task_count(uint32_t count) {
    s_mock_task_count = count;
}

} // extern "C"
