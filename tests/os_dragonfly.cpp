#include <iostream>
#include <limits>
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

void test_timeval_conversion() {
    using syscape::detail::os_backend::timeval_to_time_point;

    const auto converted = timeval_to_time_point(2, 345678);
    expect(converted &&
               converted->time_since_epoch() ==
                   std::chrono::seconds(2) + std::chrono::microseconds(345678),
           "timeval conversion must preserve microsecond precision");

    const auto invalid_fraction = timeval_to_time_point(0, 1000000);
    expect(!invalid_fraction &&
               invalid_fraction.error() == syscape::errc::malformed_data,
           "out-of-range timeval fractions must be malformed");

    const auto overflow =
        timeval_to_time_point((std::numeric_limits<std::int64_t>::max)(), 0);
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "unrepresentable boot times must report value_too_large");
}

void test_runtime_queries() {
    expect_nonempty_string(syscape::os::product_name(),
                           "product name must be nonempty");
    expect_nonempty_string(syscape::os::kernel_name(),
                           "kernel name must be nonempty");
    expect_nonempty_string(syscape::os::kernel_version(),
                           "kernel version must be nonempty");
    const auto host = syscape::os::host_name();
    expect((host && !host->empty()) || host.error() == syscape::errc::not_found,
           "host name must be nonempty or report not_found");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed && elapsed->count() >= 0,
           "uptime must be a nonnegative duration");

    const auto started = syscape::os::boot_time();
    expect(started.has_value(), "boot time query must succeed");
}

} // namespace

int main() {
    test_timeval_conversion();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
