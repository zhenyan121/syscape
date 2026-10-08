#include <iostream>
#include <string>

#include "ti/sysbios/BIOS.h"
#include "ti/sysbios/knl/Clock.h"

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
               syscape::operating_system::ti_rtos,
           "target_operating_system must report ti_rtos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for TI-RTOS");

    const auto prod = syscape::os::product_name();
#if defined(SYSCAPE_TARGET_SYSBIOS_STANDALONE)
    expect(prod.has_value() && *prod == "SYS/BIOS",
           "product name must be 'SYS/BIOS' when standalone SYS/BIOS target is "
           "set");
#else
    expect(prod.has_value() && *prod == "TI-RTOS",
           "product name must be 'TI-RTOS'");
#endif

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "SYS/BIOS",
           "kernel name must be 'SYS/BIOS'");

    const auto kver = syscape::os::kernel_version();
#if defined(SYSCAPE_TI_RTOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_TI_RTOS_VERSION_STRING,
           "kernel version must match SYSCAPE_TI_RTOS_VERSION_STRING");
#elif defined(SYSCAPE_SYSBIOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_SYSBIOS_VERSION_STRING,
           "kernel version must match SYSCAPE_SYSBIOS_VERSION_STRING");
#else
    expect(kver.has_value() && *kver == "6.83.00.00",
           "kernel version must decode to 6.83.00.00 from mock BIOS version "
           "string");
#endif

    const auto pver = syscape::os::product_version();
    expect(pver.has_value(), "product version must succeed");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on TI-RTOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 100000,
           "uptime must succeed and match mock milliseconds (100000 ticks * "
           "1000 us / 1000)");

    syscape_test_set_ti_rtos_ticks(200000U);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 200000,
           "uptime must dynamically reflect updated tick count");

    syscape_test_set_ti_rtos_tick_period(0U);
    const auto bad_uptime = syscape::os::uptime();
    expect(!bad_uptime && bad_uptime.error() == syscape::errc::malformed_data,
           "uptime must report malformed_data when tick period is 0");

    syscape_test_reset_ti_rtos_mock();

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on TI-RTOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on TI-RTOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on TI-RTOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
