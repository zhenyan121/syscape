#ifndef SYSCAPE_DETAIL_FILESYSTEM_MACOS_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_MACOS_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <sys/param.h>
#include <sys/mount.h>
#include <string.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

/// Copies one fixed-size kernel text field.
///
/// The documented statfs fields are fixed arrays that are not guaranteed to
/// carry a terminator when exactly filled, so the capacity always bounds
/// the copy.
inline std::string copy_fixed_field(const char* field,
                                    std::size_t capacity) {
    return std::string(field, ::strnlen(field, capacity));
}

/// Converts one filled statfs record into a portable mount record.
inline result<filesystem_common::mount_record> convert_statfs_entry(
    const struct ::statfs& entry) {
    filesystem_common::mount_record record;
    record.source =
        copy_fixed_field(entry.f_mntfromname, sizeof(entry.f_mntfromname));
    record.mount_point =
        copy_fixed_field(entry.f_mntonname, sizeof(entry.f_mntonname));
    record.file_system_type =
        copy_fixed_field(entry.f_fstypename, sizeof(entry.f_fstypename));
    return record;
}

/// Reads the live mount table through the documented getfsstat interface.
///
/// The buffer is owned by this call, so concurrent queries never share the
/// platform's static storage. The table can change between the sizing call
/// and the filling call, so the query retries while the population keeps
/// growing and reports temporarily_unavailable only when it stays unstable.
inline result<std::vector<filesystem_common::mount_record>> mounts() {
    constexpr int maximum_attempts = 16;

    const int initial_count = ::getfsstat(nullptr, 0, MNT_NOWAIT);
    if (initial_count < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    std::size_t expected =
        static_cast<std::size_t>(initial_count) + 1U;

    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::vector<struct ::statfs> entries(expected);
        const auto requested_bytes = entries.size() * sizeof(struct ::statfs);
        if (requested_bytes >
            static_cast<std::size_t>(
                (std::numeric_limits<int>::max)())) {
            return fail(errc::value_too_large);
        }
        const int filled = ::getfsstat(
            entries.data(), static_cast<int>(requested_bytes), MNT_NOWAIT);
        if (filled < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        // A full buffer means the table may hold more entries than were
        // copied; grow and re-read instead of silently truncating.
        if (static_cast<std::size_t>(filled) >= entries.size()) {
            expected = entries.size() * 2U;
            continue;
        }

        std::vector<filesystem_common::mount_record> records;
        records.reserve(static_cast<std::size_t>(filled));
        for (int index = 0; index < filled; ++index) {
            result<filesystem_common::mount_record> record =
                convert_statfs_entry(entries[static_cast<std::size_t>(index)]);
            if (!record) { return fail(record.error()); }
            records.push_back(std::move(*record));
        }
        return records;
    }
    return fail(errc::temporarily_unavailable);
}

/// Queries capacity for the volume containing the given path.
///
/// Path input is validated at the public boundary before backend selection.
inline result<filesystem_common::space_snapshot> space(
    const std::string& path) {
    return statvfs_space(path);
}

/// Queries the platform's maximum single-component name length for the
/// volume containing the given path through POSIX pathconf.
///
/// The value is the documented _PC_NAME_MAX record in bytes. Native
/// failures reported by the platform's limit interface are preserved.
inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    return pathconf_limit(path, _PC_NAME_MAX);
}

/// Queries the platform's recorded maximum complete-path length for the
/// volume containing the given path through POSIX pathconf.
///
/// The value is the documented _PC_PATH_MAX record in bytes. Whether the
/// platform's convention counts a terminating null byte follows that
/// platform's own limit documentation and is not normalized here. A
/// limit with no fixed value is valid data reported through the explicit
/// indeterminate flag.
inline result<filesystem_common::path_length_snapshot> max_path_length(
    const std::string& path) {
    return pathconf_limit(path, _PC_PATH_MAX);
}

/// Renders the recorded filesystem identifier pair from statfs.
///
/// Darwin reports f_fsid as two recorded words. Copying each word bit by
/// bit avoids any integer-sign conversion, and rendering indexes the
/// array in its recorded order, so the output never depends on integer
/// endianness. Filesystems that define no distinguishing identifier
/// record zeros, which are valid data rendered verbatim.
inline result<std::string> statfs_volume_id(const std::string& path) {
    for (;;) {
        struct ::statfs status {};
        if (::statfs(path.c_str(), &status) == 0) {
            static_assert(
                sizeof(status.f_fsid.val[0]) == sizeof(std::uint32_t),
                "The macOS backend requires 32-bit filesystem identifier "
                "words");
            std::uint32_t first = 0U;
            std::uint32_t second = 0U;
            ::memcpy(&first, &status.f_fsid.val[0], sizeof(first));
            ::memcpy(&second, &status.f_fsid.val[1], sizeof(second));
            return filesystem_common::render_hex_word_pair(first, second);
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

/// Returns the opaque platform-recorded identifier of the volume holding
/// the given path, rendered as sixteen lowercase hexadecimal digits.
inline result<std::string> volume_id(const std::string& path) {
    return statfs_volume_id(path);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
