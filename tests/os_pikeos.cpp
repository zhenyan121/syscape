#include <iostream>
#include <string>
#include <type_traits>

#include <p4.h>

#include <syscape/execution_environment.hpp>
#include <syscape/os.hpp>

namespace {

int failures = 0;

template <typename T>
std::string to_test_string(T&& val) {
    if constexpr (std::is_integral_v<std::decay_t<T>>) {
        return std::to_string(val);
    } else {
        return std::string(val);
    }
}

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_runtime_queries() {
    expect(syscape::target_operating_system() ==
               syscape::operating_system::pikeos,
           "target_operating_system must report pikeos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for PikeOS");

    const auto prod = syscape::os::product_name();
    expect(prod.has_value() && *prod == "PikeOS",
           "product name must be 'PikeOS'");

    const auto kname = syscape::os::kernel_name();
    expect(kname.has_value() && *kname == "PikeOS Microkernel",
           "kernel name must be 'PikeOS Microkernel'");

    const auto kver = syscape::os::kernel_version();
    const auto pver = syscape::os::product_version();
#if defined(SYSCAPE_PIKEOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_PIKEOS_VERSION_STRING,
           "kernel version must match SYSCAPE_PIKEOS_VERSION_STRING");
    expect(pver.has_value() && *pver == SYSCAPE_PIKEOS_VERSION_STRING,
           "product version must match SYSCAPE_PIKEOS_VERSION_STRING");
#elif defined(PIKEOS_VERSION_STRING)
    expect(kver.has_value() && *kver == PIKEOS_VERSION_STRING,
           "kernel version must match PIKEOS_VERSION_STRING");
    expect(pver.has_value() && *pver == PIKEOS_VERSION_STRING,
           "product version must match PIKEOS_VERSION_STRING");
#elif defined(PIKEOS_VERSION)
    expect(kver.has_value() && *kver == to_test_string(PIKEOS_VERSION),
           "kernel version must match PIKEOS_VERSION");
    expect(pver.has_value() && *pver == to_test_string(PIKEOS_VERSION),
           "product version must match PIKEOS_VERSION");
#elif defined(P4_VERSION)
    expect(kver.has_value() && *kver == to_test_string(P4_VERSION),
           "kernel version must match P4_VERSION");
    expect(pver.has_value() && *pver == to_test_string(P4_VERSION),
           "product version must match P4_VERSION");
#else
    expect(!kver && kver.error() == syscape::errc::not_supported,
           "kernel version must report not_supported on PikeOS by default");
    expect(!pver && pver.error() == syscape::errc::not_supported,
           "product version must report not_supported on PikeOS by default");
#endif

    const auto host = syscape::os::host_name();
    expect(!host && host.error() == syscape::errc::not_supported,
           "host name must report not_supported on PikeOS");

    const auto elapsed = syscape::os::uptime();
    expect(elapsed.has_value() && elapsed->count() == 1000,
           "uptime must succeed and match mock milliseconds (1000000000 ns = "
           "1000 ms)");

    syscape_test_set_pikeos_time(2500000000ULL);
    const auto elapsed2 = syscape::os::uptime();
    expect(elapsed2.has_value() && elapsed2->count() == 2500,
           "uptime must dynamically reflect updated time (2500000000 ns = 2500 "
           "ms)");

    syscape_test_set_pikeos_time_error(P4_E_STATE);
    const auto elapsed_err = syscape::os::uptime();
    expect(!elapsed_err && elapsed_err.error() == syscape::errc::io_error,
           "uptime must report io_error when p4_get_time fails");

    syscape_test_reset_pikeos_mock();

    const auto boot = syscape::os::boot_time();
    expect(!boot && boot.error() == syscape::errc::not_supported,
           "boot_time must report not_supported on PikeOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build_identifier must report not_supported on PikeOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot_identifier must report not_supported on PikeOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
