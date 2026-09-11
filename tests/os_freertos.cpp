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

void test_parsing_functions() {
    using syscape::detail::os_backend::parse_version_string;

    {
        const auto res = parse_version_string("V10.5.1\n");
        expect(res.has_value() && *res == "V10.5.1",
               "version 'V10.5.1' must parse and trim newline");
    }
    {
        const auto res = parse_version_string("  10.4.3  ");
        expect(res.has_value() && *res == "10.4.3",
               "version with whitespace must trim");
    }
    {
        const auto res = parse_version_string("");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "empty version string must report not_found");
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::freertos,
           "target_operating_system must report freertos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for FreeRTOS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "FreeRTOS",
           "product name must be 'FreeRTOS'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "FreeRTOS",
           "kernel name must be 'FreeRTOS'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "V10.5.1",
           "kernel version must be 'V10.5.1'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "V10.5.1",
           "product version must be 'V10.5.1'");

    const auto host = syscape::os::host_name();
    expect(host.error() == syscape::errc::not_supported,
           "host name must report not_supported on FreeRTOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 50000,
           "uptime must succeed and match mock tick count");

    const auto started = syscape::os::boot_time();
    expect(started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on FreeRTOS");

    const auto build = syscape::os::build_identifier();
    expect(build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on FreeRTOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on FreeRTOS");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
