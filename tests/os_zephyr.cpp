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
    using syscape::detail::os_backend::parse_host_name;
    using syscape::detail::os_backend::parse_version_string;

    // Hostname parsing
    {
        const auto res = parse_host_name("zephyrbox");
        expect(res.has_value() && *res == "zephyrbox",
               "valid hostname 'zephyrbox' must parse");
    }
    {
        const auto res = parse_host_name("box123 \r\n");
        expect(res.has_value() && *res == "box123",
               "hostname with trailing space and CRLF must trim");
    }
    {
        const auto res = parse_host_name("");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "empty hostname content must report not_found");
    }
    {
        const auto res = parse_host_name("host\nsecond");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded newline must report malformed_data");
    }

    // Version string parsing
    {
        const auto res = parse_version_string("6.0.0\n");
        expect(res.has_value() && *res == "6.0.0",
               "version '6.0.0' must parse and trim newline");
    }
    {
        const auto res = parse_version_string("  6.1.0  ");
        expect(res.has_value() && *res == "6.1.0",
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
               syscape::operating_system::zephyr,
           "target_operating_system must report zephyr");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for Zephyr");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Zephyr",
           "product name must be 'Zephyr'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "Zephyr",
           "kernel name must be 'Zephyr' in fallback");
    const auto kernel_version = syscape::os::kernel_version();
    expect(kernel_version.error() == syscape::errc::not_supported,
           "kernel version must report not_supported in fallback");

    const auto version = syscape::os::product_version();
    expect(version.error() == syscape::errc::not_supported,
           "product version must report not_supported in fallback");

    const auto host = syscape::os::host_name();
    expect(host.error() == syscape::errc::not_supported,
           "host name must report not_supported in fallback");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.error() == syscape::errc::not_supported,
           "uptime must report not_supported in fallback");

    const auto started = syscape::os::boot_time();
    expect(started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on Zephyr");

    const auto build = syscape::os::build_identifier();
    expect(build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on Zephyr");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on Zephyr");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
