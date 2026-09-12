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
               syscape::operating_system::tkernel,
           "target_operating_system must report tkernel");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for T-Kernel");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "T-Kernel",
           "product name must be 'T-Kernel'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "T-Kernel",
           "kernel name must be 'T-Kernel'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "3.0.1",
           "kernel version must be '3.0.1'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "3.0.1",
           "product version must be '3.0.1'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on T-Kernel");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 42000,
           "uptime must succeed and match mock milliseconds");

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on T-Kernel");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on T-Kernel");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on T-Kernel");
}

void test_version_decoding() {
    using syscape::detail::os_backend::detail::format_tk_version;

    {
        const auto res = format_tk_version(3001U);
        expect(res.has_value() && *res == "3.0.1",
               "version 3001 must decode to '3.0.1'");
    }
    {
        const auto res = format_tk_version(30101U);
        expect(res.has_value() && *res == "3.1.1",
               "version 30101 must decode to '3.1.1'");
    }
    {
        const auto res = format_tk_version(102U);
        expect(res.has_value() && *res == "1.0.2",
               "version 102 must decode to '1.0.2'");
    }
    {
        const auto res = format_tk_version(0U);
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "version 0 must report not_found");
    }
}

} // namespace

int main() {
    test_version_decoding();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
