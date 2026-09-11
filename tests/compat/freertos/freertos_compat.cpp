#include "FreeRTOS.h"
#include "task.h"

namespace {
TickType_t s_mock_tick_count = 50000;
size_t s_mock_free_heap = 32768;
size_t s_mock_min_free_heap = 16384;
UBaseType_t s_mock_task_priority = 5;
UBaseType_t s_mock_num_tasks = 4;
} // namespace

extern "C" {

TickType_t xTaskGetTickCount(void) {
    return s_mock_tick_count;
}

size_t xPortGetFreeHeapSize(void) {
    return s_mock_free_heap;
}

size_t xPortGetMinimumEverFreeHeapSize(void) {
    return s_mock_min_free_heap;
}

UBaseType_t uxTaskPriorityGet(TaskHandle_t /*xTaskToQuery*/) {
    return s_mock_task_priority;
}

UBaseType_t uxTaskGetNumberOfTasks(void) {
    return s_mock_num_tasks;
}

} // extern "C"
