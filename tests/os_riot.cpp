#include <iostream>
#include <string>
#include <string_view>

#include "kernel_defines.h"
#include "riot_version.h"
#include "ztimer.h"
#include "ztimer64.h"

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
               syscape::operating_system::riot,
           "target_operating_system must report riot");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for RIOT OS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "RIOT", "product name must be 'RIOT'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "RIOT", "kernel name must be 'RIOT'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "2026.01",
           "kernel version must be '2026.01'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "2026.01",
           "product version must be '2026.01'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on RIOT OS");

    const auto elapsed = syscape::os::uptime();
#if defined(RIOT_NO_TIMER)
    expect(!elapsed && elapsed.error() == syscape::errc::not_supported,
           "uptime must report not_supported when timer is disabled");
#else
    expect(elapsed.has_value() && elapsed->count() == 12345678,
           "uptime must succeed and match mock milliseconds");

    riot_mock_set_time64_ms(20000000ULL);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 20000000,
           "uptime must dynamically reflect updated time");
    riot_mock_set_time64_ms(12345678ULL);
#endif

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on RIOT OS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on RIOT OS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on RIOT OS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
