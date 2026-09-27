#include <iostream>
#include <string>

#include "rtdef.h"
#include "rtthread.h"

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
               syscape::operating_system::rtthread,
           "target_operating_system must report rtthread");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for RT-Thread");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "RT-Thread",
           "product name must be 'RT-Thread'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "RT-Thread",
           "kernel name must be 'RT-Thread'");

    const auto kver = syscape::os::kernel_version();
#if defined(RT_LEGACY_VERSION)
    expect(
        kver.has_value() && *kver == "4.1.0",
        "kernel version must be '4.1.0' for legacy RT-Thread version macros");
#else
    expect(kver.has_value() && *kver == "5.1.0",
           "kernel version must be '5.1.0'");
#endif

    const auto pver = syscape::os::product_version();
#if defined(RT_LEGACY_VERSION)
    expect(
        pver.has_value() && *pver == "4.1.0",
        "product version must be '4.1.0' for legacy RT-Thread version macros");
#else
    expect(pver.has_value() && *pver == "5.1.0",
           "product version must be '5.1.0'");
#endif

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on RT-Thread");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 12345678,
           "uptime must succeed and match mock milliseconds");

    rtthread_mock_set_tick(20000000U);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 20000000,
           "uptime must dynamically reflect updated tick");

    rtthread_mock_set_tick(0xFFFFFFFFU);
    const auto elapsed_max = syscape::os::uptime();
    expect(elapsed_max.has_value() &&
               elapsed_max->count() == static_cast<int64_t>(0xFFFFFFFFU),
           "uptime must handle max 32-bit tick count");

    rtthread_mock_set_tick(12345678U);

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on RT-Thread");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on RT-Thread");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on RT-Thread");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
