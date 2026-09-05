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

struct fake_hostname_reader {
    static syscape::result<std::string> mock_result;

    static syscape::result<std::string> read() {
        return mock_result;
    }
};

syscape::result<std::string> fake_hostname_reader::mock_result = std::string();

void test_hostname_parsing() {
    using syscape::detail::os_backend::parse_host_name;
    using syscape::detail::os_backend::parse_hostname;

    // Valid hostnames with optional trailing whitespace / newlines
    {
        const auto res = parse_host_name("redox");
        expect(res.has_value() && *res == "redox",
               "valid hostname 'redox' must parse");
    }
    {
        const auto res = parse_host_name("my-box.redox-os.org\n");
        expect(res.has_value() && *res == "my-box.redox-os.org",
               "hostname with trailing newline must trim newline");
    }
    {
        const auto res = parse_host_name("box123 \r\n");
        expect(res.has_value() && *res == "box123",
               "hostname with trailing space and CRLF must trim");
    }
    {
        const auto res = parse_host_name("node-01\t\n");
        expect(res.has_value() && *res == "node-01",
               "hostname with trailing tab and newline must trim");
    }
    {
        const auto res = parse_hostname("alias-check\n");
        expect(res.has_value() && *res == "alias-check",
               "parse_hostname alias must parse identically");
    }

    // Empty or whitespace-only -> not_found
    {
        const auto res = parse_host_name("");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "empty hostname content must report not_found");
    }
    {
        const auto res = parse_host_name("\n");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "newline-only hostname content must report not_found");
    }
    {
        const auto res = parse_host_name("\r\n");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "CRLF-only hostname content must report not_found");
    }
    {
        const auto res = parse_host_name("   \t  \r\n");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "whitespace-only hostname content must report not_found");
    }

    // Malformed data: embedded control chars, NUL, leading newlines, spaces
    {
        const auto res = parse_host_name("host\nsecond");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded newline must report malformed_data");
    }
    {
        const auto res = parse_host_name("\nhost");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with leading newline must report malformed_data");
    }
    {
        const std::string_view nul_host("host\0junk", 9);
        const auto res = parse_host_name(nul_host);
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded NUL must report malformed_data");
    }
    {
        const std::string_view leading_nul("\0host", 5);
        const auto res = parse_host_name(leading_nul);
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with leading NUL must report malformed_data");
    }
    {
        const auto res = parse_host_name("host\rsecond");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded CR must report malformed_data");
    }
    {
        const auto res = parse_host_name("\rhost");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with leading CR must report malformed_data");
    }
    {
        const auto res = parse_host_name("host\tname");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded tab must report malformed_data");
    }
    {
        const auto res = parse_host_name("\thost");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with leading tab must report malformed_data");
    }
    {
        const auto res = parse_host_name("host second");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with embedded space must report malformed_data");
    }
    {
        const auto res = parse_host_name(" host");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with leading space must report malformed_data");
    }
    {
        const std::string del_host =
            std::string("host") + static_cast<char>(127);
        const auto res = parse_host_name(del_host);
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with DEL char (127) must report malformed_data");
    }
    {
        const auto res = parse_host_name("host\x07");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with BEL char (7) must report malformed_data");
    }
    {
        const auto res = parse_host_name("host\x1f");
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "hostname with US char (31) must report malformed_data");
    }

    // Injected reader via template host_name<fake_hostname_reader>()
    {
        fake_hostname_reader::mock_result = std::string("redox-box\n");
        const auto res =
            syscape::detail::os_backend::host_name<fake_hostname_reader>();
        expect(res.has_value() && *res == "redox-box",
               "injected host_name must succeed for valid content");
    }
    {
        fake_hostname_reader::mock_result =
            syscape::fail(syscape::errc::not_found);
        const auto res =
            syscape::detail::os_backend::host_name<fake_hostname_reader>();
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "injected host_name must propagate not_found error");
    }
    {
        fake_hostname_reader::mock_result =
            syscape::fail(syscape::errc::permission_denied);
        const auto res =
            syscape::detail::os_backend::host_name<fake_hostname_reader>();
        expect(!res.has_value() &&
                   res.error() == syscape::errc::permission_denied,
               "injected host_name must propagate permission_denied error");
    }
    {
        fake_hostname_reader::mock_result = std::string("host\nsecond");
        const auto res =
            syscape::detail::os_backend::host_name<fake_hostname_reader>();
        expect(!res.has_value() && res.error() == syscape::errc::malformed_data,
               "injected host_name must report malformed_data for embedded "
               "newline");
    }
    {
        fake_hostname_reader::mock_result = std::string();
        const auto res =
            syscape::detail::os_backend::host_name<fake_hostname_reader>();
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "injected host_name must report not_found for empty content");
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::redox,
           "target_operating_system must report redox");

    expect_nonempty_string(syscape::os::product_name(),
                           "product name must be nonempty");
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
           "build identifier must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_hostname_parsing();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
