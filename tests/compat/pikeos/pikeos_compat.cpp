#include <p4.h>

static P4_time_t g_mock_time = 1000000000ULL; // 1 second in ns (1000 ms)
static P4_e_t g_mock_time_error = P4_E_OK;
static P4_prio_t g_mock_prio = 120;
static int g_mock_task_null = 0;
static P4_cpu_t g_mock_cpu_count = 4;

extern "C" {

P4_e_t p4_get_time(P4_time_t* time) {
    if (g_mock_time_error != P4_E_OK) {
        return g_mock_time_error;
    }
    if (time != nullptr) {
        *time = g_mock_time;
    }
    return P4_E_OK;
}

P4_task_t p4_task_self(void) {
    if (g_mock_task_null != 0) {
        return 0;
    }
    return 1;
}

P4_prio_t p4_task_get_prio(P4_task_t task) {
    (void)task;
    return g_mock_prio;
}

P4_cpu_t p4_cpu_count(void) {
    return g_mock_cpu_count;
}

void syscape_test_set_pikeos_time(P4_time_t time) {
    g_mock_time = time;
    g_mock_time_error = P4_E_OK;
}

void syscape_test_set_pikeos_time_error(P4_e_t err) {
    g_mock_time_error = err;
}

void syscape_test_set_pikeos_task_null(int is_null) {
    g_mock_task_null = is_null;
}

void syscape_test_set_pikeos_task_prio(P4_prio_t prio) {
    g_mock_prio = prio;
}

void syscape_test_set_pikeos_cpu_count(P4_cpu_t count) {
    g_mock_cpu_count = count;
}

void syscape_test_reset_pikeos_mock(void) {
    g_mock_time = 1000000000ULL;
    g_mock_time_error = P4_E_OK;
    g_mock_prio = 120;
    g_mock_task_null = 0;
    g_mock_cpu_count = 4;
}

} // extern "C"
