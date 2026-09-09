#include <iostream>
#include <string>

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
    using syscape::detail::os_backend::nuttx_uts_field;
    const auto valid = nuttx_uts_field("12.8.0");
    expect(valid && *valid == "12.8.0", "valid uts text must be preserved");

    const auto empty = nuttx_uts_field("");
    expect(!empty && empty.error() == syscape::errc::not_found,
           "empty uts text must report not_found");

    const char invalid[] = {static_cast<char>(0xff), '\0'};
    const auto encoding = nuttx_uts_field(invalid);
    expect(!encoding && encoding.error() == syscape::errc::invalid_encoding,
           "invalid uts text must report invalid_encoding");
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::nuttx,
           "target_operating_system must report nuttx");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for NuttX");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "NuttX",
           "product name must be 'NuttX'");

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
    expect(!started && started.error() == syscape::errc::not_supported,
           "boot time must remain unsupported without initialized wall time");

    const auto build = syscape::os::build_identifier();
    expect(build && !build->empty(),
           "build identifier must be populated from uname version");

    const auto boot_id = syscape::os::boot_identifier();
    expect(boot_id.error() == syscape::errc::not_supported,
           "boot identifier must report not_supported on NuttX");
}

} // namespace

int main() {
    test_parsing_functions();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
