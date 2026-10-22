#include <iostream>
#include <string>

#include <cyg/kernel/kapi.h>

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
               syscape::operating_system::ecos,
           "target_operating_system must report ecos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for eCos");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "eCos", "product name must be 'eCos'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "eCos Kernel",
           "kernel name must be 'eCos Kernel'");

    const auto kver = syscape::os::kernel_version();
#if defined(SYSCAPE_ECOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_ECOS_VERSION_STRING,
           "kernel version must match SYSCAPE_ECOS_VERSION_STRING");
#elif defined(CYG_VERSION_STRING)
    expect(kver.has_value() && *kver == CYG_VERSION_STRING,
           "kernel version must match CYG_VERSION_STRING");
#elif defined(CYG_VERSION_CURRENT)
    expect(kver.has_value() && *kver == std::to_string(CYG_VERSION_CURRENT),
           "kernel version must match CYG_VERSION_CURRENT");
#else
    expect(kver.has_value(), "kernel version must succeed");
#endif

    const auto pver = syscape::os::product_version();
    expect(pver.has_value(), "product version must succeed");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on eCos");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1000000,
           "uptime must succeed and match mock milliseconds (100000 ticks * "
           "1000 / 100)");

    syscape_test_set_ecos_ticks(200000ULL);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 2000000,
           "uptime must dynamically reflect updated ticks");

    syscape_test_reset_ecos_mock();

    const auto boot = syscape::os::boot_time();
    expect(!boot && boot.error() == syscape::errc::not_supported,
           "boot_time must report not_supported on eCos");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build_identifier must report not_supported on eCos");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot_identifier must report not_supported on eCos");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
