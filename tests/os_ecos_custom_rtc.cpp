#include <iostream>

#include <cyg/kernel/kapi.h>

#include <syscape/os.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_custom_rtc() {
#if defined(SYSCAPE_TEST_ECOS_1MS_TICK)
    // 1 ms tick: NUM = 1,000,000, DEN = 1
    syscape_test_set_ecos_ticks(1000ULL);
    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1000,
           "1000 ticks with 1ms RTC (NUM=1000000, DEN=1) must equal 1000 ms");

    syscape_test_set_ecos_ticks(500ULL);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 500,
           "500 ticks with 1ms RTC must equal 500 ms");

    syscape_test_reset_ecos_mock();
#elif defined(SYSCAPE_TEST_ECOS_RATE_OVERRIDE)
    // Ticks per second override = 200 Hz
    syscape_test_set_ecos_ticks(200ULL);
    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1000,
           "200 ticks at 200 Hz must equal 1000 ms");

    syscape_test_set_ecos_ticks(100ULL);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 500,
           "100 ticks at 200 Hz must equal 500 ms");

    syscape_test_reset_ecos_mock();
#endif
}

} // namespace

int main() {
    test_custom_rtc();
    return failures == 0 ? 0 : 1;
}
