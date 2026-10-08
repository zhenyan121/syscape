#include "ti/sysbios/BIOS.h"
#include "ti/sysbios/hal/Core.h"
#include "ti/sysbios/knl/Clock.h"
#include "ti/sysbios/knl/Task.h"

static uint32_t g_mock_ticks = 100000U;
uint32_t Clock_tickPeriod = 1000U; // 1000 microseconds = 1 millisecond
uint32_t ti_sysbios_hal_Core_numCores = 1U;
static int g_mock_priority = 5;
static int g_mock_task_self_null = 0;

extern "C" {

uint32_t Clock_getTicks(void) {
    return g_mock_ticks;
}

Task_Handle Task_self(void) {
    if (g_mock_task_self_null != 0) {
        return nullptr;
    }
    return reinterpret_cast<Task_Handle>(0x5678);
}

int Task_getPri(Task_Handle task) {
    if (task == nullptr) {
        return -1;
    }
    return g_mock_priority;
}

void syscape_test_set_ti_rtos_ticks(uint32_t ticks) {
    g_mock_ticks = ticks;
}

void syscape_test_set_ti_rtos_tick_period(uint32_t period_us) {
    Clock_tickPeriod = period_us;
}

void syscape_test_set_ti_rtos_task_priority(int prio) {
    g_mock_priority = prio;
}

void syscape_test_set_ti_rtos_task_self_null(int is_null) {
    g_mock_task_self_null = is_null;
}

void syscape_test_set_ti_rtos_core_count(uint32_t count) {
    ti_sysbios_hal_Core_numCores = count;
}

void syscape_test_reset_ti_rtos_mock(void) {
    g_mock_ticks = 100000U;
    Clock_tickPeriod = 1000U;
    ti_sysbios_hal_Core_numCores = 1U;
    g_mock_priority = 5;
    g_mock_task_self_null = 0;
}

} // extern "C"
