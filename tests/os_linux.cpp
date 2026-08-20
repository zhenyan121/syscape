#include <chrono>
#include <iostream>
#include <string>

#include <syscape/detail/os/linux.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/os.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expect_nonempty_utf8(const syscape::result<std::string>& value,
                          const char* message) {
    expect(value && !value->empty() && syscape::detail::is_valid_utf8(*value),
           message);
}

void test_release_parser() {
    const std::string input =
        "NAME=Example\n"
        "PRETTY_NAME=\"Example OS 1\"\n"
        "VERSION_ID='1.2'\n"
        "BUILD_ID=build-7\n";
    const auto release = syscape::detail::os_backend::parse_os_release(input);
    expect(release && release->name == "Example", "NAME must be parsed");
    expect(release && release->pretty_name == "Example OS 1",
           "quoted PRETTY_NAME must be parsed");
    expect(release && release->version_id == "1.2",
           "single-quoted VERSION_ID must be parsed");
    expect(release && release->build_id == "build-7",
           "BUILD_ID must be parsed");

    expect(!syscape::detail::os_backend::parse_os_release("BROKEN\n"),
           "malformed os-release input must fail");
    expect(!syscape::detail::os_backend::parse_os_release("NAME=\"broken\n"),
           "unterminated os-release quote must fail");

    std::string malformed_utf8(1U, static_cast<char>(0xff));
    const auto invalid_text = syscape::detail::os_common::validate_utf8(
        syscape::result<std::string>(malformed_utf8));
    expect(!invalid_text && invalid_text.error() == syscape::errc::malformed_data,
           "invalid native text must be rejected at the public boundary");
}

void test_runtime_queries() {
    expect_nonempty_utf8(syscape::os::product_name(),
                         "product name must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::kernel_name(),
                         "kernel name must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::kernel_version(),
                         "kernel version must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::host_name(),
                         "host name must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::boot_identifier(),
                         "boot identifier must be nonempty UTF-8");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed && elapsed->count() >= 0,
           "uptime must be a nonnegative duration");

    const auto started = syscape::os::boot_time();
    const auto now = std::chrono::system_clock::now();
    expect(started && *started <= now, "boot time must not be in the future");
    if (elapsed && started) {
        const auto wall_elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(now - *started);
        const auto difference = wall_elapsed > *elapsed ? wall_elapsed - *elapsed
                                                        : *elapsed - wall_elapsed;
        expect(difference < std::chrono::minutes(5),
               "boot time and uptime must agree within five minutes");
    }

    const auto version = syscape::os::product_version();
    expect(version || version.error() == syscape::errc::not_found,
           "missing product version must use not_found");
    const auto build = syscape::os::build_identifier();
    expect(build || build.error() == syscape::errc::not_found,
           "missing build identifier must use not_found");
}

} // namespace

int main() {
    test_release_parser();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
