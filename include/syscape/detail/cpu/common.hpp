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

} // namespace cpu_common
} // namespace detail
} // namespace syscape

#endif
