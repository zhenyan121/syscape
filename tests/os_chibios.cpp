#include <iostream>
#include <string>
#include <string_view>

#include "ch.h"

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
               syscape::operating_system::chibios,
           "target_operating_system must report chibios");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for ChibiOS");

    const auto prod = syscape::os::product_name();
#if defined(__CHIBIOS_NIL__)
    expect(prod.has_value() && *prod == "ChibiOS/NIL",
           "product name must be 'ChibiOS/NIL'");
#else
    expect(prod.has_value() && *prod == "ChibiOS/RT",
           "product name must be 'ChibiOS/RT'");
#endif

    const auto kname = syscape::os::kernel_name();
#if defined(__CHIBIOS_NIL__)
    expect(kname.has_value() && *kname == "ChibiOS/NIL",
           "kernel name must be 'ChibiOS/NIL'");
#else
    expect(kname.has_value() && *kname == "ChibiOS/RT",
           "kernel name must be 'ChibiOS/RT'");
#endif

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "8.0.1",
           "kernel version must be '8.0.1'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "8.0.1",
           "product version must be '8.0.1'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on ChibiOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 42000,
           "uptime must succeed and match mock milliseconds");

    ch_mock_set_systime(100000);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 100000,
           "uptime must dynamically reflect updated ticks");
    ch_mock_set_systime(42000);

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on ChibiOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on ChibiOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on ChibiOS");
}

void test_version_formatting() {
    using syscape::detail::os_backend::detail::format_chibios_version_numbers;

    {
        const auto res = format_chibios_version_numbers(8, 0, 1);
        expect(res.has_value() && *res == "8.0.1",
               "version 8, 0, 1 must format to '8.0.1'");
    }
    {
        const auto res = format_chibios_version_numbers(21, 11, 3);
        expect(res.has_value() && *res == "21.11.3",
               "version 21, 11, 3 must format to '21.11.3'");
    }
    {
        const auto res = format_chibios_version_numbers(-1, 0, 0);
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "negative version must report malformed_data");
    }
}

} // namespace

int main() {
    test_version_formatting();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
