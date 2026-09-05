#include <iostream>
#include <string>

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

void test_runtime_queries() {
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
           "build identifier must report not_supported on SerenityOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
