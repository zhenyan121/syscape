#ifndef SYSCAPE_DETAIL_CPU_COMMON_HPP
#define SYSCAPE_DETAIL_CPU_COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_common {

using cache_kind = ::syscape::cpu::cache_kind;

/// Cumulative processor time buckets shared by the Hosted backends.
///
/// Every field is a monotonically growing total observed since the platform
/// started accounting. One tick is whatever time unit the queried platform
/// uses for processor-time accounting; only differences between two
/// snapshots carry portable meaning.
struct usage_information {
    /// Cumulative time the platform attributes to ordinary user execution,
    /// including the platform's nice or low-priority user states.
    std::uint64_t user_ticks = 0U;
    /// Cumulative time the platform attributes to kernel or privileged
    /// execution on behalf of the workload.
    std::uint64_t system_ticks = 0U;
    /// Cumulative time the platform records its processors as idle.
    std::uint64_t idle_ticks = 0U;
};

/// One distinct processor cache instance shared by a recorded set of online
/// logical processors, shared by the Hosted backends awaiting boundary
/// conversion.
struct cache_entry {
    /// Cache level counted from one for the level nearest the processor.
    std::uint32_t level = 0U;
    /// Recorded kind of stored information.
    cache_kind kind = cache_kind::unified;
    /// Size of one shared instance in bytes; always positive.
    std::uint64_t instance_size_bytes = 0U;
    /// Coherency (line) size in bytes; always positive.
    std::uint32_t line_size_bytes = 0U;
    /// Associativity expressed in ways. Zero means the platform reports no
    /// value, because a real cache always has at least one way.
    std::uint32_t associativity_ways = 0U;
    /// Number of sets. Zero means the platform reports no value, because a
    /// real cache always has at least one set.
    std::uint32_t sets_count = 0U;
    /// Online logical processors that share one such instance. Zero means
    /// the platform reports no sharing count, because an unreported count
    /// cannot be derived without inventing platform facts.
    std::uint32_t shared_logical_processor_count = 0U;
};

inline result<std::vector<std::string>> validate_utf8_labels(
    result<std::vector<std::string>> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    for (const std::string& label : *value) {
        if (label.empty() || !is_valid_utf8(label)) {
            return fail(errc::malformed_data);
        }
    }
    return value;
}

/// Validates converted cache entries at the public boundary.
///
/// Every entry needs a positive level, a positive instance size, and a
/// positive line size, because none of those quantities has a meaningful
/// zero; the geometry and sharing fields use the documented zero to record
/// an unreported platform value. Entries must be sorted by nondecreasing
/// level and then by kind so that callers can rely on the documented order.
inline result<std::vector<cache_entry>> validate_cache_entries(
    result<std::vector<cache_entry>> entries) {
    if (!entries) { return fail(entries.error()); }
    bool have_previous = false;
    std::uint32_t previous_level = 0U;
    cache_kind previous_kind = cache_kind::data;
    for (const cache_entry& entry : *entries) {
        if (entry.level == 0U || entry.instance_size_bytes == 0U ||
            entry.line_size_bytes == 0U) {
            return fail(errc::malformed_data);
        }
        if (have_previous &&
            (entry.level < previous_level ||
             (entry.level == previous_level &&
              entry.kind < previous_kind))) {
            return fail(errc::malformed_data);
        }
        have_previous = true;
        previous_level = entry.level;
        previous_kind = entry.kind;
    }
    return entries;
}

} // namespace cpu_common
} // namespace detail
} // namespace syscape

#endif
