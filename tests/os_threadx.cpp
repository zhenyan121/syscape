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

void test_parsing_functions() {
    using syscape::detail::os_backend::detail::parse_version_string;

    {
        const auto res =
            parse_version_string("Eclipse ThreadX Version 6.4.0\n");
        expect(res.has_value() && *res == "6.4.0",
               "version '6.4.0' must parse and trim prefix and newline");
    }
    {
        const auto res = parse_version_string("Version 6.2.1");
        expect(res.has_value() && *res == "6.2.1",
               "version with prefix must parse");
    }
    {
        const auto res = parse_version_string("");
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "empty version string must report not_found");
    }
    {
        const auto res = parse_version_string(nullptr);
        expect(!res.has_value() && res.error() == syscape::errc::not_found,
               "null version string must report not_found");
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::threadx,
           "target_operating_system must report threadx");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for ThreadX");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "Eclipse ThreadX",
           "product name must be 'Eclipse ThreadX'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "ThreadX",
           "kernel name must be 'ThreadX'");

    const auto kver = syscape::os::kernel_version();
    expect(kver.has_value() && *kver == "6.4.0",
           "kernel version must be '6.4.0'");

    const auto pver = syscape::os::product_version();
    expect(pver.has_value() && *pver == "6.4.0",
           "product version must be '6.4.0'");

    const auto host = syscape::os::host_name();
    expect(host.error() == syscape::errc::not_supported,
           "host name must report not_supported on ThreadX");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1234560,
           "uptime must succeed and match mock tick count");

    const auto started = syscape::os::boot_time();
    expect(started.error() == syscape::errc::not_supported,
           "boot time query must report not_supported on ThreadX");

    const auto build = syscape::os::build_identifier();
    expect(build.error() == syscape::errc::not_supported,
           "build identifier must report not_supported on ThreadX");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
