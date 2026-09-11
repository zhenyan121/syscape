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
               syscape::operating_system::embos,
           "target_operating_system must report embos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for embOS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "SEGGER embOS",
           "product name must be 'SEGGER embOS'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "embOS",
           "kernel name must be 'embOS'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "5.18.1",
           "kernel version must be '5.18.1'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "5.18.1",
           "product version must be '5.18.1'");

    const auto host = syscape::os::host_name();
    expect(host.error() == syscape::errc::not_supported,
           "host name must report not_supported on embOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 42000,
           "uptime must succeed and match mock tick count");

    const auto started = syscape::os::boot_time();
    expect(started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on embOS");

    const auto build = syscape::os::build_identifier();
    expect(build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on embOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on embOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
