#include <iostream>
#include <string>
#include <string_view>

#include "os/os.h"

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

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::mynewt,
           "target_operating_system must report mynewt");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for Apache Mynewt");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Apache Mynewt",
           "product name must be 'Apache Mynewt'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "Mynewt OS",
           "kernel name must be 'Mynewt OS'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "1.10.0",
           "kernel version must be '1.10.0'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "1.10.0",
           "product version must be '1.10.0'");

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on Apache Mynewt");

    const auto elapsed = syscape::os::uptime();
#if defined(MYNEWT_NO_TICKS_PER_SEC)
    expect(!elapsed && elapsed.error() == syscape::errc::not_supported,
           "uptime must report not_supported when frequency macros are not "
           "defined");
#else
    expect(elapsed.has_value() && elapsed->count() == 50000,
           "uptime must succeed and match mock milliseconds");

    mynewt_mock_set_time(100000);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 100000,
           "uptime must dynamically reflect updated ticks");
    mynewt_mock_set_time(50000);
#endif

    const auto started = syscape::os::boot_time();
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on Apache Mynewt");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on Apache Mynewt");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on Apache Mynewt");
}

void test_version_formatting() {
    using syscape::detail::os_backend::detail::format_mynewt_version_numbers;

    {
        const auto res = format_mynewt_version_numbers(1U, 10U, 0U);
        expect(res.has_value() && *res == "1.10.0",
               "version 1, 10, 0 must format to '1.10.0'");
    }
    {
        const auto res = format_mynewt_version_numbers(2U, 0U, 1U);
        expect(res.has_value() && *res == "2.0.1",
               "version 2, 0, 1 must format to '2.0.1'");
    }
}

} // namespace

int main() {
    test_runtime_queries();
    test_version_formatting();
    return failures == 0 ? 0 : 1;
}
