#ifndef SYSCAPE_DETAIL_PROCESS_COMMON_HPP
#define SYSCAPE_DETAIL_PROCESS_COMMON_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_common {

/// CPU execution-time amounts consumed by the calling process.
struct cpu_time_usage {
    /// Time spent executing in user mode. Never negative.
    std::chrono::nanoseconds user;
    /// Time spent executing in kernel mode on behalf of the process.
    /// Never negative.
    std::chrono::nanoseconds system;
};

/// Resident and virtual memory extents of the calling process in bytes.
struct memory_usage_snapshot {
    /// Physical memory currently occupied by the process, as defined by the
    /// platform's resident-set concept.
    std::uint64_t resident_bytes;
    /// The process's virtual-memory extent as defined by the platform; the
    /// precise meaning (address-space size or committed extent) is documented
    /// per backend source.
    std::uint64_t virtual_bytes;
};

/// Selects the recorded process resource limit a backend query reports.
///
/// The meaning and unit of every member value is determined by the selected
/// kind and is documented by the public enumeration that maps onto it.
enum class limit_resource {
    /// Maximum core-file size in bytes.
    core_file_size,
    /// Maximum accumulated CPU time in seconds.
    cpu_time,
    /// Maximum size in bytes of a single file the process may write.
    file_size,
    /// Maximum number of open file descriptors.
    open_files,
    /// Maximum size in bytes of the process stack segment.
    stack_size,
    /// Maximum total virtual address-space extent in bytes.
    address_space,
};

/// One recorded bound of a process resource limit.
struct resource_limit_bound {
    /// The recorded bound in the unit named by the queried kind. Meaningful
    /// only when unlimited is false.
    std::uint64_t amount = 0U;
    /// True when the platform records no bound instead of a finite amount.
    bool unlimited = false;
};

/// Soft and hard bounds of one process resource limit.
struct resource_limit_snapshot {
    /// The currently enforced bound.
    resource_limit_bound soft;
    /// The ceiling to which an unprivileged process may raise the soft bound.
    resource_limit_bound hard;
};

inline result<std::string> validate_utf8_path(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty() || !is_valid_utf8(*value)) {
        return value->empty() ? fail(errc::malformed_data)
                              : fail(errc::invalid_encoding);
    }
    return value;
}

inline result<std::vector<std::string>> validate_utf8_arguments(
    result<std::vector<std::string>> value) {
    if (!value) { return fail(value.error()); }
    for (const std::string& argument : *value) {
        if (!is_valid_utf8(argument)) { return fail(errc::invalid_encoding); }
    }
    return value;
}

} // namespace process_common
} // namespace detail
} // namespace syscape

#endif
