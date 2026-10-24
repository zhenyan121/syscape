#include <iostream>
#include <string>
#include <type_traits>

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
               syscape::operating_system::lynxos,
           "target_operating_system must report lynxos");
    expect(syscape::target_execution_environment() ==
               syscape::execution_environment::rtos,
           "target_execution_environment must report rtos for LynxOS");

    const auto prod = syscape::os::product_name();
#if defined(LYNXOS_178) || defined(__LYNXOS_178__)
    expect(prod.has_value() && *prod == "LynxOS-178",
           "product name must be 'LynxOS-178'");
#else
    expect(prod.has_value() && *prod == "LynxOS",
           "product name must be 'LynxOS'");
#endif

    const auto kname = syscape::os::kernel_name();
#if defined(LYNXOS_178) || defined(__LYNXOS_178__)
    expect(kname.has_value() && *kname == "LynxOS-178 Microkernel",
           "kernel name must be 'LynxOS-178 Microkernel'");
#else
    expect(kname.has_value() && *kname == "LynxOS Kernel",
           "kernel name must be 'LynxOS Kernel'");
#endif

    const auto kver = syscape::os::kernel_version();
    const auto pver = syscape::os::product_version();
#if defined(SYSCAPE_LYNXOS_VERSION_STRING)
    expect(kver.has_value() && *kver == SYSCAPE_LYNXOS_VERSION_STRING,
           "kernel version must match SYSCAPE_LYNXOS_VERSION_STRING");
    expect(pver.has_value() && *pver == SYSCAPE_LYNXOS_VERSION_STRING,
           "product version must match SYSCAPE_LYNXOS_VERSION_STRING");
#elif defined(LYNXOS_VERSION_STRING)
    expect(kver.has_value() && *kver == LYNXOS_VERSION_STRING,
           "kernel version must match LYNXOS_VERSION_STRING");
    expect(pver.has_value() && *pver == LYNXOS_VERSION_STRING,
           "product version must match LYNXOS_VERSION_STRING");
#elif defined(LYNXOS_VERSION)
#if defined(SYSCAPE_TEST_LYNXOS_VERSION_INVALID)
    expect(!kver && kver.error() == syscape::errc::not_found,
           "kernel version must report not_found for invalid version");
    expect(!pver && pver.error() == syscape::errc::not_found,
           "product version must report not_found for invalid version");
#else
    expect(kver.has_value() && *kver == to_test_string(LYNXOS_VERSION),
           "kernel version must match LYNXOS_VERSION");
    expect(pver.has_value() && *pver == to_test_string(LYNXOS_VERSION),
           "product version must match LYNXOS_VERSION");
#endif
#elif defined(_LYNXOS_VERSION)
    expect(kver.has_value() && *kver == to_test_string(_LYNXOS_VERSION),
           "kernel version must match _LYNXOS_VERSION");
    expect(pver.has_value() && *pver == to_test_string(_LYNXOS_VERSION),
           "product version must match _LYNXOS_VERSION");
#elif defined(__LYNXOS_VERSION__)
    expect(kver.has_value() && *kver == to_test_string(__LYNXOS_VERSION__),
           "kernel version must match __LYNXOS_VERSION__");
    expect(pver.has_value() && *pver == to_test_string(__LYNXOS_VERSION__),
           "product version must match __LYNXOS_VERSION__");
#else
    expect(!kver && kver.error() == syscape::errc::not_supported,
           "kernel version must report not_supported on LynxOS by default");
    expect(!pver && pver.error() == syscape::errc::not_supported,
           "product version must report not_supported on LynxOS by default");
#endif

    const auto host = syscape::os::host_name();
    if (host.has_value()) {
        expect(!host->empty(), "host name must not be empty if reported");
    } else {
        expect(host.error() == syscape::errc::not_supported ||
                   host.error() == syscape::errc::permission_denied ||
                   host.error() == syscape::errc::value_too_large ||
                   host.error().category() == std::generic_category(),
               "host_name error must be a recognized error code");
    }

    const auto elapsed = syscape::os::uptime();
    if (elapsed.has_value()) {
        expect(elapsed->count() >= 0, "uptime must not be negative");
    } else {
        expect(elapsed.error() == syscape::errc::not_supported ||
                   elapsed.error() == syscape::errc::permission_denied ||
                   elapsed.error() == syscape::errc::not_found ||
                   elapsed.error().category() == std::generic_category(),
               "uptime error must report a recognized error code");
    }

    const auto boot = syscape::os::boot_time();
    expect(!boot && boot.error() == syscape::errc::not_supported,
           "boot_time must report not_supported on LynxOS");

    const auto build = syscape::os::build_identifier();
    expect(!build && build.error() == syscape::errc::not_supported,
           "build_identifier must report not_supported on LynxOS");

    const auto boot_id = syscape::os::boot_identifier();
    expect(!boot_id && boot_id.error() == syscape::errc::not_supported,
           "boot_identifier must report not_supported on LynxOS");
}

} // namespace

int main() {
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
