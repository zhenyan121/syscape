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
        const auto res = parse_host_name("fuchsiabox");
        expect(res.has_value() && *res == "fuchsiabox",
               "valid hostname 'fuchsiabox' must parse");
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
        const auto res = parse_version_string("24.20240101.0\n");
        expect(res.has_value() && *res == "24.20240101.0",
               "version '24.20240101.0' must parse and trim newline");
    }
    {
        const auto res = parse_version_string("  1.0.0  ");
        expect(res.has_value() && *res == "1.0.0",
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
               syscape::operating_system::fuchsia,
           "target_operating_system must report fuchsia");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::sandboxed,
           "target_execution_environment must report sandboxed for Fuchsia");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Fuchsia",
           "product name must be 'Fuchsia'");

    expect_nonempty_string(syscape::os::kernel_name(),
                           "kernel name must be nonempty");
    const auto kernel_version = syscape::os::kernel_version();
    expect((kernel_version && !kernel_version->empty()) ||
               kernel_version.error() == syscape::errc::not_found,
           "kernel version must be nonempty or report not_found");

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
    expect(started.has_value() ||
               started.error() == syscape::errc::not_supported,
           "boot_time must succeed or report not_supported");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on Fuchsia");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
