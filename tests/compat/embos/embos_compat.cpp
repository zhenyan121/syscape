#include "RTOS.h"

namespace {

OS_TIME s_mock_ticks = 42000;
OS_PRIO s_mock_priority = 15;
// In embOS, tasks represent threads of execution.
OS_UINT s_mock_tasks_threads = 8;
OS_U32 s_mock_cores = 4;
OS_U32 s_mock_version = 51801; // 5.18.1
OS_U32 s_mock_free_heap = 32768U;

} // namespace

extern "C" {

OS_U32 OS_GetVersion(void) {
    return s_mock_version;
}

OS_TIME OS_TIME_GetTicks(void) {
    return s_mock_ticks;
}

OS_TIME OS_GetTime32(void) {
    return s_mock_ticks;
}

OS_U32 OS_TIME_ConvertTicks2ms(OS_TIME Ticks) {
#if defined(OS_TICK_FREQ) && (OS_TICK_FREQ > 0)
    return static_cast<OS_U32>(
        (static_cast<unsigned long long>(Ticks) * 1000ULL) / OS_TICK_FREQ);
#else
    return static_cast<OS_U32>(Ticks);
#endif
}

OS_U32 OS_CORE_GetNumCores(void) {
    return s_mock_cores;
}

OS_PRIO OS_TASK_GetPriority(OS_CONST_PTR OS_TASK* pTask) {
    (void)pTask;
    return s_mock_priority;
}

OS_PRIO OS_GetPriority(OS_CONST_PTR OS_TASK* pTask) {
    (void)pTask;
    return s_mock_priority;
}

OS_UINT OS_TASK_GetNumTasks(void) {
    return s_mock_tasks_threads;
}

OS_U32 OS_GetFreeHeapSpace(void) {
    return s_mock_free_heap;
}

} // extern "C"
