#include <p4.h>

static P4_time_t g_mock_time = 1000000000ULL; // 1 second in ns (1000 ms)

extern "C" {

P4_time_t p4_get_time(void) {
    return g_mock_time;
}

void syscape_test_set_pikeos_time(P4_time_t time) {
    g_mock_time = time;
}

void syscape_test_reset_pikeos_mock(void) {
    g_mock_time = 1000000000ULL;
}

} // extern "C"
