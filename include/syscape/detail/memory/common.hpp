#ifndef SYSCAPE_DETAIL_MEMORY_COMMON_HPP
#define SYSCAPE_DETAIL_MEMORY_COMMON_HPP

#include <cstdint>
#include <limits>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_common {

/// Swap or pagefile capacity values shared by the memory backends.
///
/// Both fields are byte counts. Zero totals are valid data that mean the
/// platform has no configured paging space; they are not error sentinels.
struct swap_usage {
    /// Configured swap or pagefile capacity in bytes.
    std::uint64_t total_bytes = 0U;
    /// Unused swap or pagefile capacity in bytes.
    std::uint64_t free_bytes = 0U;
};

/// Rejects a swap snapshot whose unused capacity exceeds its total.
inline result<swap_usage> validate_swap_usage(result<swap_usage> value) {
    if (!value) { return fail(value.error()); }
    if (value->free_bytes > value->total_bytes) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Virtual-memory commit accounting values shared by the memory backends.
///
/// Both fields are byte counts at the moment of the query. There is
/// deliberately no cross-field ordering validation: platforms with heuristic
/// overcommit legitimately report a committed amount above the recorded
/// limit, so such a rendering is valid data.
struct commit_usage {
    /// Currently committed virtual memory in bytes.
    std::uint64_t committed_bytes = 0U;
    /// The platform's effective commit limit in bytes. Depending on the
    /// platform this limit can be scoped to the system or to the calling
    /// process, whichever the source defines; backends document the scope.
    std::uint64_t commit_limit_bytes = 0U;
};

/// Huge-page pool counts shared by the memory backends.
///
/// Both fields count pages of the platform's default huge-page size. Zero is
/// valid data that means the platform maintains an empty huge-page pool;
/// it is not an error sentinel.
struct huge_page_pool_usage {
    /// Configured pool size in huge pages, including any dynamically grown
    /// surplus population the platform records as part of the pool.
    std::uint64_t total_count = 0U;
    /// Unallocated pool pages. Pages reserved by future allocations remain
    /// part of this count where the platform records them that way.
    std::uint64_t free_count = 0U;
};

/// Rejects a huge-page pool snapshot whose free count exceeds its total.
///
/// Platforms document that a page cannot be free outside the pool, so a
/// contradictory rendering is malformed platform data rather than valid
/// transient state.
inline result<huge_page_pool_usage> validate_huge_page_pool(
    result<huge_page_pool_usage> value) {
    if (!value) { return fail(value.error()); }
    if (value->free_count > value->total_count) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Validates a platform's default huge-page size.
///
/// A huge-page size is always a positive power-of-two byte count. Keeping
/// this check at the portable boundary prevents malformed platform data from
/// escaping as a value that violates the public contract.
inline result<std::uint64_t> validate_huge_page_size(
    result<std::uint64_t> value) {
    if (!value) { return fail(value.error()); }
    if (*value == 0U || (*value & (*value - 1U)) != 0U) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Converts a used-over-total ratio into a whole percentage rounded half up.
///
/// Both arguments are dimensionless counts of any shared unit, for example
/// bytes or pages. The exact integer computation rejects totals whose
/// scaling would overflow instead of silently losing precision, so callers
/// report value_too_large for unrepresentably large inputs.
inline result<std::uint32_t> utilization_percent(
    std::uint64_t used_units, std::uint64_t total_units) {
    constexpr std::uint64_t scale = 100U;
    if (total_units == 0U) { return fail(errc::malformed_data); }
    // Guard both the scaled numerator and the rounding addition: keeping
    // total_units within one two-hundredth of the range bounds every
    // intermediate product below the maximum representable value.
    if (total_units >
            (std::numeric_limits<std::uint64_t>::max)() / (2U * scale)) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t percent =
        (used_units * scale + total_units / 2U) / total_units;
    if (percent > scale) { return fail(errc::malformed_data); }
    return static_cast<std::uint32_t>(percent);
}

/// One pressure-stall window shared by the memory backends.
///
/// Each average is scaled by one million so the platform's two-fractional-
/// digit percentage rendering converts without any loss: 12.34 percent
/// becomes 12340000 micro-percent. The total accumulates stall time in
/// microseconds. Zero values are valid data that mean no stall occurred;
/// they are not error sentinels.
struct pressure_sample {
    /// Fraction of wall-clock time with stalls over the last ten seconds,
    /// in micro-percent (percent multiplied by exactly one million).
    std::uint64_t average10_micro_percent = 0U;
    /// Fraction of wall-clock time with stalls over the last sixty seconds,
    /// in micro-percent.
    std::uint64_t average60_micro_percent = 0U;
    /// Fraction of wall-clock time with stalls over the last three hundred
    /// seconds, in micro-percent.
    std::uint64_t average300_micro_percent = 0U;
    /// Cumulative stall duration since boot in microseconds.
    std::uint64_t total_microseconds = 0U;
};

/// Pressure-stall snapshot for tasks stalled on memory, shared by the
/// memory backends.
///
/// The some record describes intervals in which at least one runnable task
/// stalled on memory; the full record describes intervals in which all
/// concurrently runnable tasks stalled simultaneously. The presence flag is
/// retained so future backends can express an honestly optional full record
/// without changing the shared representation. Linux memory-pressure
/// snapshots always set it because their documented format requires both
/// records.
struct pressure_status {
    /// Stalls affecting at least one task.
    pressure_sample some;
    /// Whether the platform exposed a full-stall record.
    bool has_full = false;
    /// Stalls affecting every concurrent task; meaningful only when
    /// has_full is true.
    pressure_sample full;
};

} // namespace memory_common
} // namespace detail
} // namespace syscape

#endif
