#include <chrono>
#include <iostream>
#include <string>

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

void test_runtime_queries() {
    expect_nonempty_utf8(syscape::os::product_name(),
                         "product name must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::kernel_name(),
                         "kernel name must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::kernel_version(),
                         "kernel version must be nonempty UTF-8");
    expect_nonempty_utf8(syscape::os::host_name(),
                         "host name must be nonempty UTF-8");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed && elapsed->count() >= 0,
           "uptime must be a nonnegative duration");

    const auto started = syscape::os::boot_time();
    const auto now = std::chrono::system_clock::now();
    expect(started && *started <= now, "boot time must not be in the future");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
