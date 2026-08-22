#ifndef SYSCAPE_DETAIL_FILESYSTEM_COMMON_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_COMMON_HPP

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_common {

/// Mounted-filesystem record shared by the filesystem backends.
///
/// Every field is platform text awaiting boundary validation. An empty
/// source is valid data because platforms may record mounts without a
/// named backing device.
struct mount_record {
    std::string source;
    std::string mount_point;
    std::string file_system_type;
};

/// Volume capacity values shared by the filesystem backends.
///
/// All byte counts are computed from the platform's native counters. The
/// consistency checks live at the common boundary so that every backend
/// enforces the same contract.
struct space_snapshot {
    /// Total capacity of the volume in bytes.
    std::uint64_t capacity_bytes = 0U;
    /// Unused volume capacity in bytes.
    std::uint64_t free_bytes = 0U;
    /// Unused capacity available to unprivileged callers in bytes.
    std::uint64_t available_bytes = 0U;
    /// Allocation granularity of the volume in bytes.
    std::uint64_t block_size_bytes = 0U;
    /// True when the volume rejects write access.
    bool read_only = false;
};

/// Validates mount records at the public boundary.
///
/// A usable record needs a non-empty mount point and file-system type in
/// valid UTF-8. The source may be empty where the platform records no
/// device. One unusable record fails the whole snapshot so that silently
/// dropping entries can never hide platform damage.
inline result<std::vector<mount_record>> validate_mount_records(
    result<std::vector<mount_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const mount_record& entry : *records) {
        if (entry.mount_point.empty() || entry.file_system_type.empty()) {
            return fail(errc::malformed_data);
        }
        if (!is_valid_utf8(entry.source) ||
            !is_valid_utf8(entry.mount_point) ||
            !is_valid_utf8(entry.file_system_type)) {
            return fail(errc::invalid_encoding);
        }
    }
    return records;
}

/// Validates caller-supplied path text before any backend runs.
///
/// Empty text and embedded null characters are not paths accepted by the
/// native interfaces. This is the single enforcement point for space() input,
/// applied by the public header before backend selection, so every platform
/// backend - including the generic fallback - observes identical
/// invalid-argument and invalid-encoding behavior instead of each defining
/// its own.
inline result<void> validate_space_path(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(path)) { return fail(errc::invalid_encoding); }
    return {};
}

/// Validates a volume-capacity snapshot at the public boundary.
///
/// A zero block size cannot express any byte count on that volume and is
/// malformed platform data. Free or unprivately-available capacity beyond
/// total capacity contradicts the platform's own accounting and is also
/// malformed rather than clamped.
inline result<space_snapshot> validate_space_snapshot(
    result<space_snapshot> value) {
    if (!value) { return fail(value.error()); }
    if (value->block_size_bytes == 0U) {
        return fail(errc::malformed_data);
    }
    if (value->free_bytes > value->capacity_bytes ||
        value->available_bytes > value->capacity_bytes) {
        return fail(errc::malformed_data);
    }
    return value;
}

} // namespace filesystem_common
} // namespace detail
} // namespace syscape

#endif
