#ifndef SYSCAPE_DETAIL_LINUX_PROCESS_METRICS_HPP
#define SYSCAPE_DETAIL_LINUX_PROCESS_METRICS_HPP

#include <chrono>
#include <cstdint>
#include <limits>

#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace linux_process_metrics {

inline result<std::chrono::nanoseconds> ticks_to_nanoseconds(
    std::uint64_t ticks, long ticks_per_second) {
    if (ticks_per_second <= 0 ||
        static_cast<std::uint64_t>(ticks_per_second) >
            (std::numeric_limits<std::uint64_t>::max)() / 1000000000ULL) {
        return fail(errc::not_supported);
    }
    const std::uint64_t rate = static_cast<std::uint64_t>(ticks_per_second);
    const std::uint64_t whole_seconds = ticks / rate;
    constexpr std::uint64_t maximum_whole_seconds = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count() / 1000000000);
    if (whole_seconds > maximum_whole_seconds) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t remainder_ticks = ticks % rate;
    const std::uint64_t remainder_nanoseconds =
        remainder_ticks * 1000000000ULL / rate;
    const std::uint64_t whole_nanoseconds = whole_seconds * 1000000000ULL;
    constexpr std::uint64_t maximum_nanoseconds = static_cast<std::uint64_t>(
        (std::chrono::nanoseconds::max)().count());
    if (whole_nanoseconds > maximum_nanoseconds - remainder_nanoseconds) {
        return fail(errc::value_too_large);
    }
    return std::chrono::nanoseconds(whole_nanoseconds + remainder_nanoseconds);
}

inline result<std::uint64_t> scale_resident_bytes(
    std::uint64_t pages, std::uint64_t page_size) {
    if (page_size == 0U) {
        return fail(errc::malformed_data);
    }
    if (pages > (std::numeric_limits<std::uint64_t>::max)() / page_size) {
        return fail(errc::value_too_large);
    }
    return pages * page_size;
}

inline result<std::chrono::system_clock::time_point> compose_start_time(
    std::chrono::system_clock::time_point boot_time,
    std::chrono::nanoseconds age) {
    using clock = std::chrono::system_clock;
    const std::chrono::nanoseconds boot_nanoseconds =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            boot_time.time_since_epoch());
    if (age.count() < 0 ||
        boot_nanoseconds.count() >
            (std::chrono::nanoseconds::max)().count() - age.count()) {
        return fail(errc::value_too_large);
    }
    return boot_time + std::chrono::duration_cast<clock::duration>(age);
}

} // namespace linux_process_metrics
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_LINUX_PROCESS_METRICS_HPP
