#include <cyg/kernel/kapi.h>

static cyg_tick_count_t g_mock_ticks = 100000ULL;
static cyg_priority_t g_mock_priority = 10;
static int g_mock_thread_null = 0;

extern "C" {

cyg_tick_count_t cyg_current_time(void) {
    return g_mock_ticks;
}

cyg_handle_t cyg_thread_self(void) {
    if (g_mock_thread_null != 0) {
        return 0;
    }
    return 1;
}

cyg_priority_t cyg_thread_get_priority(cyg_handle_t thread) {
    (void)thread;
    return g_mock_priority;
}

cyg_priority_t cyg_thread_get_current_priority(cyg_handle_t thread) {
    (void)thread;
    return g_mock_priority;
}

void syscape_test_set_ecos_ticks(cyg_tick_count_t ticks) {
    g_mock_ticks = ticks;
}

void syscape_test_set_ecos_thread_priority(cyg_priority_t prio) {
    g_mock_priority = prio;
}

void syscape_test_set_ecos_thread_null(int is_null) {
    g_mock_thread_null = is_null;
}

void syscape_test_reset_ecos_mock(void) {
    g_mock_ticks = 100000ULL;
    g_mock_priority = 10U;
    g_mock_thread_null = 0;
}
}
