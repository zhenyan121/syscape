#include "INTEGRITY.h"

namespace {

Time s_mock_time = 42000000000ULL; // 42,000,000,000 ns = 42,000 ms
ProcessorCount s_mock_processors = 4;
Task s_mock_task = 1;
Priority s_mock_priority = 15;
MemorySize s_mock_free_mem = 32768U;

} // namespace

extern "C" {

Error GetTime(Time* time) {
    if (time != nullptr) {
        *time = s_mock_time;
        return Success;
    }
    return Failure;
}

Error GetProcessorCount(ProcessorCount* count) {
    if (count != nullptr) {
        *count = s_mock_processors;
        return Success;
    }
    return Failure;
}

Error CurrentTask(Task* task) {
    if (task != nullptr) {
        *task = s_mock_task;
        return Success;
    }
    return Failure;
}

Error GetPriority(Task task, Priority* priority) {
    if (task == s_mock_task && priority != nullptr) {
        *priority = s_mock_priority;
        return Success;
    }
    return Failure;
}

MemorySize GetMemoryPoolFreeSpace(void) {
    return s_mock_free_mem;
}

} // extern "C"
