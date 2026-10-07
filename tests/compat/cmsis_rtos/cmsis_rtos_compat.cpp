#include "cmsis_os2.h"
#include <cstring>

static uint32_t g_mock_tick_count = 50000U;
static uint32_t g_mock_tick_freq = 1000U;
#if defined(osCMSIS_RTX)
static uint32_t g_mock_kernel_version = 40820000U;
static char g_mock_id_str[64] = "RTX V4.82";
#else
static uint32_t g_mock_kernel_version = 50050001U;
static char g_mock_id_str[64] = "RTX V5.5.1";
#endif
static osPriority_t g_mock_priority = osPriorityNormal;
static uint32_t g_mock_thread_count = 4U;
static osStatus_t g_mock_info_status = osOK;

extern "C" {

osStatus_t osKernelGetInfo(osVersion_t* version, char* id_buf,
                           uint32_t id_size) {
    if (g_mock_info_status != osOK) {
        return g_mock_info_status;
    }
    if (version != nullptr) {
        version->api = 20001000U;
        version->kernel = g_mock_kernel_version;
    }
    if (id_buf != nullptr && id_size > 0U) {
        std::strncpy(id_buf, g_mock_id_str, id_size - 1U);
        id_buf[id_size - 1U] = '\0';
    }
    return osOK;
}

osKernelState_t osKernelGetState(void) {
    return osKernelRunning;
}

uint32_t osKernelGetTickCount(void) {
    return g_mock_tick_count;
}

uint32_t osKernelGetTickFreq(void) {
    return g_mock_tick_freq;
}

uint32_t osKernelGetSysTickCount(void) {
    return g_mock_tick_count * 1000U;
}

uint32_t osKernelGetSysTickFreq(void) {
    return g_mock_tick_freq * 1000U;
}

uint32_t osKernelSysTick(void) {
    return g_mock_tick_count;
}

osThreadId_t osThreadGetId(void) {
    return reinterpret_cast<osThreadId_t>(0x1234);
}

osPriority_t osThreadGetPriority(osThreadId_t thread_id) {
    (void)thread_id;
    return g_mock_priority;
}

uint32_t osThreadGetCount(void) {
    return g_mock_thread_count;
}

void syscape_test_set_cmsis_rtos_tick_count(uint32_t ticks) {
    g_mock_tick_count = ticks;
}

void syscape_test_set_cmsis_rtos_tick_freq(uint32_t freq) {
    g_mock_tick_freq = freq;
}

void syscape_test_set_cmsis_rtos_kernel_version(uint32_t kernel_ver,
                                                const char* id_str) {
    g_mock_kernel_version = kernel_ver;
    if (id_str != nullptr) {
        std::strncpy(g_mock_id_str, id_str, sizeof(g_mock_id_str) - 1U);
        g_mock_id_str[sizeof(g_mock_id_str) - 1U] = '\0';
    } else {
        g_mock_id_str[0] = '\0';
    }
}

void syscape_test_set_cmsis_rtos_thread_priority(osPriority_t prio) {
    g_mock_priority = prio;
}

void syscape_test_set_cmsis_rtos_thread_count(uint32_t count) {
    g_mock_thread_count = count;
}

void syscape_test_set_cmsis_rtos_kernel_info_status(osStatus_t status) {
    g_mock_info_status = status;
}

void syscape_test_reset_cmsis_rtos_mock(void) {
    g_mock_tick_count = 50000U;
    g_mock_tick_freq = 1000U;
#if defined(osCMSIS_RTX)
    g_mock_kernel_version = 40820000U;
    std::strncpy(g_mock_id_str, "RTX V4.82", sizeof(g_mock_id_str) - 1U);
#else
    g_mock_kernel_version = 50050001U;
    std::strncpy(g_mock_id_str, "RTX V5.5.1", sizeof(g_mock_id_str) - 1U);
#endif
    g_mock_id_str[sizeof(g_mock_id_str) - 1U] = '\0';
    g_mock_priority = osPriorityNormal;
    g_mock_thread_count = 4U;
    g_mock_info_status = osOK;
}

} // extern "C"
