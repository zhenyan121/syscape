#include <iostream>
#include <string>
#include <string_view>

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
               syscape::operating_system::integrity,
           "target_operating_system must report integrity");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for INTEGRITY");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Green Hills INTEGRITY",
           "product name must be 'Green Hills INTEGRITY'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "INTEGRITY",
           "kernel name must be 'INTEGRITY'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "11.7.8",
           "kernel version must be '11.7.8'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "11.7.8",
           "product version must be '11.7.8'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on INTEGRITY");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 42000,
           "uptime must succeed and match mock nanoseconds / 1e6");

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on INTEGRITY");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on INTEGRITY");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on INTEGRITY");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
