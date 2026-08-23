#ifndef SYSCAPE_DETAIL_RESOURCE_COMMON_HPP
#define SYSCAPE_DETAIL_RESOURCE_COMMON_HPP

#include <cmath>
#include <cstdint>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_common {

/// System load samples shared by the resource backends.
///
/// Load averages are dimensionless exponentially damped measures of demand
/// for the system's processors. Zero is valid data for an idle system.
struct load_samples {
    /// Exponentially damped load average over the last one minute.
    double one_minute = 0.0;
    /// Exponentially damped load average over the last five minutes.
    double five_minute = 0.0;
    /// Exponentially damped load average over the last fifteen minutes.
    double fifteen_minute = 0.0;
};

/// Scheduler entity counts shared by the resource backends.
struct entity_counts {
    /// Number of entities currently runnable on a processor.
    std::uint64_t runnable = 0U;
    /// Total number of schedulable entities that currently exist.
    std::uint64_t schedulable = 0U;
};

/// Rejects load samples that no platform can legitimately report.
///
/// A negative, non-finite, or missing sample is malformed platform data;
/// zero samples are valid data for an idle system.
inline result<load_samples> validate_load_samples(result<load_samples> value) {
    if (!value) { return fail(value.error()); }
    const double fields[3] = {value->one_minute, value->five_minute,
                              value->fifteen_minute};
    for (const double field : fields) {
        if (!std::isfinite(field) || field < 0.0) {
            return fail(errc::malformed_data);
        }
    }
    return value;
}

/// Rejects scheduler counts whose runnable entities exceed the population.
inline result<entity_counts> validate_entity_counts(result<entity_counts> value) {
    if (!value) { return fail(value.error()); }
    if (value->runnable > value->schedulable) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Rejects a count that cannot describe the running system.
///
/// The calling process and its calling thread always exist while a query
/// runs, so a zero process or thread count cannot be valid data and is
/// malformed platform data rather than an empty-but-valid result.
inline result<std::uint64_t> validate_positive_count(result<std::uint64_t> value) {
    if (!value) { return fail(value.error()); }
    if (*value == 0U) { return fail(errc::malformed_data); }
    return value;
}

} // namespace resource_common
} // namespace detail
} // namespace syscape

#endif
