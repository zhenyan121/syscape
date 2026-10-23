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
#if defined(SYSCAPE_TEST_PIKEOS_VERSION_NOT_SUPPORTED)
    expect(!kver && kver.error() == syscape::errc::not_supported,
           "kernel version must report not_supported");
    expect(!pver && pver.error() == syscape::errc::not_supported,
           "product version must report not_supported");
#elif defined(SYSCAPE_TEST_PIKEOS_VERSION_NOT_FOUND)
    expect(!kver && kver.error() == syscape::errc::not_found,
           "kernel version must report not_found");
    expect(!pver && pver.error() == syscape::errc::not_found,
           "product version must report not_found");
#else
    expect(kver.has_value() && *kver == to_test_string(PIKEOS_VERSION),
           "kernel version must match PIKEOS_VERSION");
    expect(pver.has_value() && *pver == to_test_string(PIKEOS_VERSION),
           "product version must match PIKEOS_VERSION");
#endif
#elif defined(P4_VERSION)
#if defined(SYSCAPE_TEST_P4_VERSION_NOT_SUPPORTED)
    expect(!kver && kver.error() == syscape::errc::not_supported,
           "kernel version must report not_supported when P4_VERSION is "
           "detection flag 1");
    expect(!pver && pver.error() == syscape::errc::not_supported,
           "product version must report not_supported when P4_VERSION is "
           "detection flag 1");
#elif defined(SYSCAPE_TEST_P4_VERSION_NOT_FOUND)
    expect(!kver && kver.error() == syscape::errc::not_found,
           "kernel version must report not_found when P4_VERSION is <= 0");
    expect(!pver && pver.error() == syscape::errc::not_found,
           "product version must report not_found when P4_VERSION is <= 0");
#else
    expect(kver.has_value() && *kver == to_test_string(P4_VERSION),
           "kernel version must match P4_VERSION");
    expect(pver.has_value() && *pver == to_test_string(P4_VERSION),
           "product version must match P4_VERSION");
#endif
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
#if defined(SYSCAPE_PIKEOS_TICKS_PER_SECOND)
    constexpr std::uint64_t rate1 =
        static_cast<std::uint64_t>(SYSCAPE_PIKEOS_TICKS_PER_SECOND);
    const std::int64_t expected1 =
        static_cast<std::int64_t>((1000000000ULL * 1000ULL) / rate1);
    expect(elapsed.has_value() && elapsed->count() == expected1,
           "uptime must match configured SYSCAPE_PIKEOS_TICKS_PER_SECOND");
#elif defined(PIKEOS_TICKS_PER_SECOND)
    constexpr std::uint64_t rate1 =
        static_cast<std::uint64_t>(PIKEOS_TICKS_PER_SECOND);
    const std::int64_t expected1 =
        static_cast<std::int64_t>((1000000000ULL * 1000ULL) / rate1);
    expect(elapsed.has_value() && elapsed->count() == expected1,
           "uptime must match configured PIKEOS_TICKS_PER_SECOND");
#else
    expect(elapsed.has_value() && elapsed->count() == 1000,
           "uptime must succeed and match mock milliseconds (1000000000 ns = "
           "1000 ms)");
#endif

    syscape_test_set_pikeos_time(2500000000ULL);
    const auto elapsed2 = syscape::os::uptime();
#if defined(SYSCAPE_PIKEOS_TICKS_PER_SECOND)
    constexpr std::uint64_t rate2 =
        static_cast<std::uint64_t>(SYSCAPE_PIKEOS_TICKS_PER_SECOND);
    const std::int64_t expected2 =
        static_cast<std::int64_t>((2500000000ULL * 1000ULL) / rate2);
    expect(
        elapsed2.has_value() && elapsed2->count() == expected2,
        "uptime must dynamically reflect updated time under custom tick rate");
#elif defined(PIKEOS_TICKS_PER_SECOND)
    constexpr std::uint64_t rate2 =
        static_cast<std::uint64_t>(PIKEOS_TICKS_PER_SECOND);
    const std::int64_t expected2 =
        static_cast<std::int64_t>((2500000000ULL * 1000ULL) / rate2);
    expect(
        elapsed2.has_value() && elapsed2->count() == expected2,
        "uptime must dynamically reflect updated time under custom tick rate");
#else
    expect(elapsed2.has_value() && elapsed2->count() == 2500,
           "uptime must dynamically reflect updated time (2500000000 ns = 2500 "
           "ms)");
#endif

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
