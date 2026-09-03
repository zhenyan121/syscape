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

void test_os_queries() {
    expect_nonempty_string(syscape::os::product_name(),
                           "product name must be nonempty");
    expect_nonempty_string(syscape::os::kernel_name(),
                           "kernel name must be nonempty");
    const auto version = syscape::os::kernel_version();
    expect((version && !version->empty()) ||
               version.error() == syscape::errc::not_found,
           "kernel version must be nonempty or not_found");
    const auto host = syscape::os::host_name();
    expect((host && !host->empty()) || host.error() == syscape::errc::not_found,
           "host name must be nonempty or not_found");
    const auto elapsed = syscape::os::uptime();
    expect((elapsed && elapsed->count() >= 0) ||
               elapsed.error() == syscape::errc::not_supported,
           "uptime must be nonnegative duration or not_supported");
    const auto started = syscape::os::boot_time();
    expect(started.has_value() ||
               started.error() == syscape::errc::not_supported,
           "boot time must succeed or report not_supported");
}

} // namespace

int main() {
    test_os_queries();
    return failures == 0 ? 0 : 1;
}
