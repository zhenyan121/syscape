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

void expect_nonempty_string(const syscape::result<std::string>& value,
                            const char* message) {
    expect(value && !value->empty(), message);
}

void test_parsing_functions() {
    using syscape::detail::os_backend::parse_host_name;
    using syscape::detail::os_backend::parse_version_string;

    // Hostname parsing
    {
        const auto res = parse_host_name("vxbox");
        expect(res.has_value() && *res == "vxbox",
               "valid hostname 'vxworksbox' must parse");
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
        const auto res = parse_version_string("7.1.0\n");
        expect(res.has_value() && *res == "7.1.0",
               "version '7.1.0' must parse and trim newline");
    }
    {
        const auto res = parse_version_string("  8.0.0  ");
        expect(res.has_value() && *res == "8.0.0",
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
               syscape::operating_system::vxworks,
           "target_operating_system must report vxworks");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for VxWorks");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "VxWorks",
           "product name must be 'VxWorks'");

    expect_nonempty_string(syscape::os::kernel_name(),
                           "kernel name must be nonempty");
    expect_nonempty_string(syscape::os::kernel_version(),
                           "kernel version must be nonempty");

    const auto version = syscape::os::product_version();
    expect((version && !version->empty()) ||
               version.error() == syscape::errc::not_found,
           "product version must be nonempty or report not_found");

    const auto host = syscape::os::host_name();
    expect((host && !host->empty()) || host.error() == syscape::errc::not_found,
           "host name must be nonempty or report not_found");

    const auto elapsed = syscape::os::uptime();
    expect((elapsed && elapsed->count() >= 0) ||
               elapsed.error() == syscape::errc::not_found ||
               elapsed.error() == syscape::errc::not_supported,
           "uptime must be a nonnegative duration or report error");

    const auto started = syscape::os::boot_time();
    expect(started.has_value() || started.error() == syscape::errc::not_found ||
               started.error() == syscape::errc::not_supported,
           "boot time query must succeed or report error");

    const auto build = syscape::os::build_identifier();
    expect(build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on VxWorks");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on VxWorks");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
