#include <iostream>
#include <string>

#include "nucleus.h"

#include <syscape/execution_environment.hpp>
#include <syscape/os.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::nucleus,
           "target_operating_system must report nucleus");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for Nucleus RTOS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Nucleus RTOS",
           "product name must be 'Nucleus RTOS'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "Nucleus PLUS",
           "kernel name must be 'Nucleus PLUS'");

    const auto kver = syscape::os::kernel_version();
#if defined(SYSCAPE_NUCLEUS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_NUCLEUS_VERSION_STRING,
           "kernel version must match SYSCAPE_NUCLEUS_VERSION_STRING");
#else
    expect(
        kver.has_value() && *kver == "3.2.1",
        "kernel version must decode to 3.2.1 from mock Nucleus release string");
#endif

    const auto pver = syscape::os::product_version();
    expect(pver.has_value(), "product version must succeed");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on Nucleus RTOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1000000,
           "uptime must succeed and match mock milliseconds (100000 ticks * "
           "1000 / 100)");

    syscape_test_set_nucleus_ticks(200000U);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 2000000,
           "uptime must dynamically reflect updated tick count");

    syscape_test_reset_nucleus_mock();

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on Nucleus RTOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on Nucleus RTOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on Nucleus RTOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
