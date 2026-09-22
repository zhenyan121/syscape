#include <iostream>
#include <string>
#include <string_view>

#include "mbed.h"

#include <syscape/execution_environment.hpp>
#include <syscape/os.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::mbed,
           "target_operating_system must report mbed");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for Mbed OS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Arm Mbed OS",
           "product name must be 'Arm Mbed OS'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "Mbed OS",
           "kernel name must be 'Mbed OS'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "6.15.0",
           "kernel version must be '6.15.0'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "6.15.0",
           "product version must be '6.15.0'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on Mbed OS");

    const auto elapsed = syscape::os::uptime();
#if defined(MBED_NO_TICK_FREQ)
    expect(!elapsed && elapsed.error() == syscape::errc::not_supported,
           "uptime must report not_supported when frequency macros are not "
           "defined");
#else
    expect(elapsed.has_value() && elapsed->count() == 5000,
           "uptime must succeed and match mock milliseconds");

    mbed_mock_set_tick_count(10000);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 10000,
           "uptime must dynamically reflect updated ticks");
    mbed_mock_set_tick_count(5000);

#if !defined(MBED_USE_TICK_FREQ_MACRO) &&                                      \
    !defined(MBED_CONF_RTOS_TICK_FREQ) && !defined(OS_TICK_FREQ)
    mbed_mock_set_tick_freq(0U);
    const auto zero_freq = syscape::os::uptime();
    expect(!zero_freq && zero_freq.error() == syscape::errc::malformed_data,
           "uptime must report malformed_data when tick frequency is 0");
    mbed_mock_set_tick_freq(1000U);
#endif
#endif

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on Mbed OS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on Mbed OS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on Mbed OS");
}

void test_version_formatting() {
    using syscape::detail::os_backend::detail::format_mbed_version_numbers;

    {
        const auto res = format_mbed_version_numbers(6U, 15U, 0U);
        expect(res.has_value() && *res == "6.15.0",
               "version 6, 15, 0 must format to '6.15.0'");
    }
    {
        const auto res = format_mbed_version_numbers(5U, 12U, 4U);
        expect(res.has_value() && *res == "5.12.4",
               "version 5, 12, 4 must format to '5.12.4'");
    }
}

} // namespace

int main() {
    test_runtime_queries();
    test_version_formatting();
    return failures == 0 ? 0 : 1;
}
