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

/// One recorded path-length bound shared by the filesystem backends.
///
/// Platforms that define no fixed bound for a limit report that outcome
/// as valid data instead of an error, so the indeterminate state is part
/// of the value rather than a failure code.
struct path_length_snapshot {
    /// The platform's recorded bound. Meaningful only when indeterminate
    /// is false; validation normalizes the value to zero otherwise.
    std::uint64_t length = 0U;
    /// True when the platform documents no fixed bound for the limit.
    bool indeterminate = false;
};

/// Renders one 32-bit word as exactly eight lowercase hexadecimal digits.
///
/// Volume identifiers are opaque platform-recorded words, and a fixed
/// width keeps renderings comparable within one platform.
inline std::string render_hex32(std::uint32_t value) {
    static const char digits[] = "0123456789abcdef";
    std::string rendered(8U, '0');
    for (std::size_t index = 0; index < 8U; ++index) {
        const unsigned int shift = static_cast<unsigned int>(28U - 4U * index);
        rendered[index] = digits[(value >> shift) & 0xFU];
    }
    return rendered;
}

/// Renders two recorded 32-bit identifier words in documented order.
inline std::string render_hex_word_pair(std::uint32_t first,
                                        std::uint32_t second) {
    std::string rendered = render_hex32(first);
    rendered += render_hex32(second);
    return rendered;
}

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
/// native interfaces. This is the single enforcement point for every
/// path-taking filesystem query, applied by the public header before
/// backend selection, so every platform backend - including the generic
/// fallback - observes identical invalid-argument and invalid-encoding
/// behavior instead of each defining its own.
inline result<void> validate_path_input(const std::string& path) {
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

/// Validates a path-length bound at the public boundary.
///
/// An indeterminate limit is valid data and normalizes its length to
/// zero so the pair is always self-consistent. A determinate bound of
/// zero cannot name even one path component and contradicts any running
/// filesystem, so it is malformed platform data rather than a plausible
/// value.
inline result<path_length_snapshot> validate_path_length(
    result<path_length_snapshot> value) {
    if (!value) { return fail(value.error()); }
    if (value->indeterminate) {
        value->length = 0U;
        return value;
    }
    if (value->length == 0U) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Validates a rendered volume identifier at the public boundary.
///
/// Every backend renders its platform's recorded identifier word or word
/// pair at a fixed width, so an empty rendering means the backend failed
/// to record anything and is malformed platform data. A rendering of
/// zero digits is valid data wherever the platform records zeros,
/// because some platforms define no distinguishing identifier for
/// certain filesystems and zero is then their honest answer.
inline result<std::string> validate_volume_id(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    return value;
}

} // namespace filesystem_common
} // namespace detail
} // namespace syscape

#endif
