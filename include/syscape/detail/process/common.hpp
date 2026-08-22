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
