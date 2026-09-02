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

void test_timespec_conversion() {
    using syscape::detail::os_backend::boot_time_from_clocks;
    using syscape::detail::os_backend::timespec_to_time_point;
    using syscape::detail::os_backend::timespec_to_uptime;

    const auto converted = timespec_to_time_point(2, 345678901);
    expect(converted && converted->time_since_epoch() ==
                            std::chrono::seconds(2) +
                                std::chrono::nanoseconds(345678901),
           "timespec conversion must preserve nanosecond precision");

    const auto invalid_fraction = timespec_to_time_point(0, 1000000000);
    expect(!invalid_fraction &&
               invalid_fraction.error() == syscape::errc::malformed_data,
           "out-of-range timespec fractions must be malformed");

    const auto overflow =
        timespec_to_time_point((std::numeric_limits<std::int64_t>::max)(), 0);
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "unrepresentable boot times must report value_too_large");

    const auto elapsed = timespec_to_uptime(2, 345678901);
    expect(elapsed && *elapsed == std::chrono::milliseconds(2345),
           "uptime conversion must truncate only sub-millisecond precision");

    const auto invalid_uptime = timespec_to_uptime(-1, 0);
    expect(!invalid_uptime &&
               invalid_uptime.error() == syscape::errc::malformed_data,
           "negative uptime must be malformed");

    const auto derived = boot_time_from_clocks(100, 100000000, 2, 300000000);
    expect(derived && derived->time_since_epoch() ==
                          std::chrono::seconds(97) +
                              std::chrono::nanoseconds(800000000),
           "clock subtraction must normalize negative nanoseconds");

    const auto inconsistent = boot_time_from_clocks(1, 0, 2, 0);
    expect(!inconsistent &&
               inconsistent.error() == syscape::errc::malformed_data,
           "uptime later than realtime must be malformed");
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
    expect(started || started.error() == syscape::errc::malformed_data,
           "boot time must succeed or reject inconsistent platform clocks");
}

} // namespace

int main() {
    test_timespec_conversion();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
