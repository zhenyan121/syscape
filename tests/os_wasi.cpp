#include <iostream>
#include <string>

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
               syscape::operating_system::wasi,
           "target_operating_system must report wasi");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::sandboxed,
           "target_execution_environment must report sandboxed for WASI");

    const auto prod = syscape::os::product_name();
    expect(!prod && prod.error() == syscape::errc::not_supported,
           "product name must report not_supported at runtime on WASI");

    const auto kernel = syscape::os::kernel_name();
    expect(!kernel && kernel.error() == syscape::errc::not_supported,
           "kernel name must report not_supported at runtime on WASI");

    const auto kernel_ver = syscape::os::kernel_version();
    expect(!kernel_ver && kernel_ver.error() == syscape::errc::not_supported,
           "kernel version must report not_supported at runtime on WASI");

    const auto prod_ver = syscape::os::product_version();
    expect(!prod_ver && prod_ver.error() == syscape::errc::not_supported,
           "product version must report not_supported");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on WASI sandbox");

    const auto uptime = syscape::os::uptime();
    expect(!uptime && uptime.error() == syscape::errc::not_supported,
           "uptime must report not_supported on WASI");

    const auto boot = syscape::os::boot_time();
    expect(!boot && boot.error() == syscape::errc::not_supported,
           "boot time must report not_supported on WASI");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on WASI");

    const auto build_id = syscape::os::build_identifier();
    expect(!build_id && build_id.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on WASI");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
