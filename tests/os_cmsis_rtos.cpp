#include <iostream>
#include <string>

#if defined(__CMSIS_RTOS) && !defined(__CMSIS_RTOS2)
#include "cmsis_os.h"
#else
#include "cmsis_os2.h"
#endif

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
               syscape::operating_system::cmsis_rtos,
           "target_operating_system must report cmsis_rtos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for CMSIS-RTOS");

    const auto prod = syscape::os::product_name();
#if defined(osCMSIS_RTX5) || defined(osCMSIS_RTX) || defined(SYSCAPE_TARGET_RTX)
    expect(prod.has_value() && *prod == "Keil RTX",
           "product name must be 'Keil RTX' when RTX macro is present");
#else
    expect(prod.has_value() && *prod == "CMSIS-RTOS",
           "product name must be 'CMSIS-RTOS'");
#endif

    const auto kname = syscape::os::kernel_name();
#if defined(osCMSIS_RTX5)
    expect(kname.has_value() && *kname == "RTX5", "kernel name must be 'RTX5'");
#elif defined(osCMSIS_RTX)
    expect(kname.has_value() && *kname == "RTX", "kernel name must be 'RTX'");
#else
    expect(kname.has_value() && *kname == "CMSIS-RTOS",
           "kernel name must be 'CMSIS-RTOS'");
#endif

    const auto kver = syscape::os::kernel_version();
#if defined(SYSCAPE_CMSIS_RTOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_CMSIS_RTOS_VERSION_STRING,
           "kernel version must match SYSCAPE_CMSIS_RTOS_VERSION_STRING");
#elif defined(SYSCAPE_RTX_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_RTX_VERSION_STRING,
           "kernel version must match SYSCAPE_RTX_VERSION_STRING");
#elif defined(osCMSIS_RTX)
    expect(kver.has_value() && *kver == "4.82.0",
           "kernel version must decode to 4.82.0 for RTX v1");
#else
    expect(kver.has_value() && *kver == "5.5.1",
           "kernel version must decode to 5.5.1 from mock kernel version");
#endif

    const auto pver = syscape::os::product_version();
    expect(pver.has_value(), "product version must succeed");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on CMSIS-RTOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 50000,
           "uptime must succeed and match mock milliseconds");

    syscape_test_set_cmsis_rtos_tick_count(100000U);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 100000,
           "uptime must dynamically reflect updated tick count");

#if defined(osCMSIS) && (osCMSIS >= 0x20000U)
    syscape_test_set_cmsis_rtos_tick_freq(0U);
    const auto bad_uptime = syscape::os::uptime();
    expect(!bad_uptime && bad_uptime.error() == syscape::errc::malformed_data,
           "uptime must report malformed_data when tick freq is 0");
#endif

    syscape_test_reset_cmsis_rtos_mock();

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on CMSIS-RTOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on CMSIS-RTOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on CMSIS-RTOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
