#include "nucleus.h"
#include <cstring>

static uint32_t g_mock_ticks = 100000U;
static uint32_t g_mock_core_count = 1U;
static uint32_t g_mock_established_tasks = 4U;
static uint8_t g_mock_priority = 10U;
static int g_mock_task_null = 0;
static char g_mock_version[64] = "3.2.1";
static NU_TASK g_mock_task = {"main", 10U, 1U};

extern "C" {

char* NU_Release_Information(void) {
    return g_mock_version;
}

uint32_t NU_Get_Time_Stamp(void) {
    return g_mock_ticks;
}

uint32_t NU_Retrieve_Clock(void) {
    return g_mock_ticks;
}

NU_TASK* NU_Current_Task_Pointer(void) {
    if (g_mock_task_null != 0) {
        return nullptr;
    }
    return &g_mock_task;
}

int NU_Task_Information(NU_TASK* task, char* name, uint8_t* status,
                        uint32_t* scheduled_count, uint8_t* priority,
                        void** stack_base, uint32_t* stack_size,
                        uint32_t* minimum_stack) {
    if (task == nullptr) {
        return -1;
    }
    if (name != nullptr) {
        // In Nucleus PLUS, task names are fixed 8-character fields
        // (NU_MAX_NAME). Real SDK NU_Task_Information writes the characters and
        // a null terminator.
        std::memcpy(name, task->tc_name, 8);
        name[8] = '\0';
    }
    if (status != nullptr) {
        *status = 0;
    }
    if (scheduled_count != nullptr) {
        *scheduled_count = 100U;
    }
    if (priority != nullptr) {
        *priority = g_mock_priority;
    }
    if (stack_base != nullptr) {
        *stack_base = nullptr;
    }
    if (stack_size != nullptr) {
        *stack_size = 4096U;
    }
    if (minimum_stack != nullptr) {
        *minimum_stack = 1024U;
    }
    return NU_SUCCESS;
}

uint32_t NU_Established_Tasks(void) {
    return g_mock_established_tasks;
}

uint32_t NU_Core_Count(void) {
    return g_mock_core_count;
}

void syscape_test_set_nucleus_ticks(uint32_t ticks) {
    g_mock_ticks = ticks;
}

void syscape_test_set_nucleus_task_priority(uint8_t prio) {
    g_mock_priority = prio;
    g_mock_task.tc_priority = prio;
}

void syscape_test_set_nucleus_current_task_null(int is_null) {
    g_mock_task_null = is_null;
}

void syscape_test_set_nucleus_established_tasks(uint32_t count) {
    g_mock_established_tasks = count;
}

void syscape_test_set_nucleus_core_count(uint32_t count) {
    g_mock_core_count = count;
}

void syscape_test_set_nucleus_version_string(const char* ver) {
    if (ver != nullptr) {
        std::strncpy(g_mock_version, ver, sizeof(g_mock_version) - 1);
        g_mock_version[sizeof(g_mock_version) - 1] = '\0';
    } else {
        g_mock_version[0] = '\0';
    }
}

void syscape_test_reset_nucleus_mock(void) {
    g_mock_ticks = 100000U;
    g_mock_core_count = 1U;
    g_mock_established_tasks = 4U;
    g_mock_priority = 10U;
    g_mock_task_null = 0;
    std::strncpy(g_mock_version, "3.2.1", sizeof(g_mock_version) - 1);
    g_mock_version[sizeof(g_mock_version) - 1] = '\0';
    // Note: tc_name in NU_TASK_STRUCT is a fixed 8-byte buffer rather than
    // null-terminated.
    std::strncpy(g_mock_task.tc_name, "main", sizeof(g_mock_task.tc_name));
    g_mock_task.tc_priority = 10U;
    g_mock_task.tc_id = 1U;
}

} // extern "C"
