#ifndef SYSCAPE_FILESYSTEM_HPP
#define SYSCAPE_FILESYSTEM_HPP

/// @file
/// @brief Hosted mounted-filesystem and volume-capacity queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux enumerates mounts from the kernel-documented
/// /proc/self/mounts interface and queries capacity through POSIX statvfs.
/// macOS and FreeBSD enumerate mounts through the documented getfsstat
/// interface and query capacity through statvfs. Windows enumerates
/// drive-letter volumes
/// through GetLogicalDrives, QueryDosDeviceW, and GetVolumeInformationW,
/// resolves queried paths to their real volume mount point through
/// GetVolumePathNameW, and queries capacity through GetDiskFreeSpaceExW;
/// network shares without drive letters and other non-drive-letter volumes
/// are not enumerated by this slice. Other targets use the generic
/// not-supported fallback.
/// @note Path-limit queries use POSIX pathconf on Linux, macOS, and FreeBSD.
/// Windows reads the documented MaximumComponentLength record of
/// GetVolumeInformationW after resolving a path to its volume, and reports
/// no per-volume maximum complete-path length because the platform bounds
/// complete paths by process-wide activation policy instead.
/// @note Volume-identifier queries render each platform's recorded opaque
/// identifier verbatim: Linux uses the kernel-documented statfs f_fsid word
/// pair, macOS and FreeBSD use the statfs f_fsid word pair, and Windows uses
/// the documented GetVolumeInformationW serial number.
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/filesystem.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/filesystem/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/filesystem/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/filesystem/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/filesystem/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/filesystem/openbsd.hpp>
#else
#include <syscape/detail/filesystem/generic.hpp>
#endif

namespace syscape {
namespace filesystem {

/// One entry of the platform's mounted-filesystem table.
struct mount_entry {
    /// Source device or pseudo-device backing the mount, reported verbatim.
    /// Pseudo-filesystems name their driver here, for example "proc" or
    /// "tmpfs". The field is empty where the platform records no source;
    /// an empty source is valid data and not an error.
    std::string source;
    /// Absolute path at which the filesystem is attached. The path is
    /// reported verbatim without canonicalization and can refer to a
    /// location that was unmounted or removed after the snapshot.
    std::string mount_point;
    /// File-system type name reported by the operating system, for example
    /// "ext4", "ntfs", or "apfs".
    std::string file_system_type;
};

/// Capacity of one mounted volume at the moment of the query.
struct space_info {
    /// Total capacity of the volume in bytes.
    std::uint64_t capacity_bytes;
    /// Unused capacity in bytes, including space reserved for privileged
    /// users on platforms that define such a reservation.
    std::uint64_t free_bytes;
    /// Unused capacity available to unprivileged callers in bytes, as
    /// estimated by the operating system. This is normally less than or
    /// equal to free_bytes but remains a platform-defined estimate.
    std::uint64_t available_bytes;
    /// Allocation granularity reported for the volume in bytes. POSIX
    /// platforms report the fundamental file-system block size (f_frsize,
    /// falling back to f_bsize when the platform records zero); Windows
    /// reports its cluster size as the closest documented equivalent.
    std::uint64_t block_size_bytes;
    /// True when the volume rejects write access at the time of the query.
    /// The state can change between calls, for example when media are
    /// physically switched to read-only or a lock is toggled.
    bool read_only;
};

/// Returns a snapshot of the platform's mounted-filesystem table.
///
/// The snapshot reflects the table as observed during the call; concurrent
/// mounts and unmounts become visible only to later calls. Per-volume
/// capacity requires the separate space() query.
///
/// Platform behavior differs: POSIX enumeration reads the kernel mount
/// table only and never touches the listed volumes, so unresponsive network
/// filesystems cannot block it. Windows enumeration queries information for
/// each drive-letter volume through documented interfaces, which contacts
/// those volumes; letters backed by unready media or unresponsive network
/// shares may block or surface native errors until the device responds.
///
/// On Windows this slice enumerates drive-letter volumes only; network
/// shares and mounted folders that lack drive letters, plus other
/// non-drive-letter volume representations, require additional platform
/// facilities and are omitted.
///
/// @return A list with at least one entry on any running hosted system,
/// malformed_data for unusable platform records, invalid_encoding when a
/// record is not valid UTF-8, temporarily_unavailable when the platform
/// mount table keeps changing during enumeration, not_supported when the
/// platform exposes no acceptable source, or a native platform error.
inline result<std::vector<mount_entry>> mounts() {
    result<std::vector<detail::filesystem_common::mount_record>> records =
        detail::filesystem_common::validate_mount_records(
            detail::filesystem_backend::mounts());
    if (!records) { return fail(records.error()); }
    std::vector<mount_entry> entries;
    entries.reserve(records->size());
    for (detail::filesystem_common::mount_record& record : *records) {
        mount_entry entry;
        entry.source = std::move(record.source);
        entry.mount_point = std::move(record.mount_point);
        entry.file_system_type = std::move(record.file_system_type);
        entries.push_back(std::move(entry));
    }
    return entries;
}

/// Returns capacity information for the volume containing the given path.
///
/// Every component of the query reports the state observed during the call;
/// values change continuously with system activity. The path may name an
/// existing file or directory; the platform resolves it to the enclosing
/// volume. The input must be valid UTF-8. Relative paths are interpreted
/// against the current working directory where the platform supports them.
///
/// @param path An existing file or directory path in UTF-8.
/// @return A capacity snapshot whose free_bytes never exceeds
/// capacity_bytes, invalid_argument for an empty path or one containing an
/// embedded null character, invalid_encoding when the path is not valid
/// UTF-8, not_supported when the platform exposes no acceptable source,
/// malformed_data for inconsistent or degenerate platform data such as a
/// zero block size, or a native platform error such as a missing path or
/// denied permission.
inline result<space_info> space(const std::string& path) {
    const result<void> valid =
        detail::filesystem_common::validate_path_input(path);
    if (!valid) { return fail(valid.error()); }
    result<detail::filesystem_common::space_snapshot> snapshot =
        detail::filesystem_common::validate_space_snapshot(
            detail::filesystem_backend::space(path));
    if (!snapshot) { return fail(snapshot.error()); }
    return space_info{snapshot->capacity_bytes, snapshot->free_bytes,
                      snapshot->available_bytes,
                      snapshot->block_size_bytes, snapshot->read_only};
}

/// One recorded path-length bound of a volume.
struct path_length_limit {
    /// The platform's recorded bound. Meaningful only when indeterminate
    /// is false; always zero when indeterminate is true. The unit is the
    /// platform's own recorded length unit and is documented by each
    /// query rather than normalized across platforms.
    std::uint64_t length;
    /// True when the platform documents no fixed bound for the limit.
    /// This is valid platform data, not an error.
    bool indeterminate;
};

/// Returns the platform's recorded maximum length of one path component
/// on the volume containing the given path.
///
/// A path component is the text between two path separators, such as a
/// single file or directory name. The bound is a property of the volume
/// and changes only when the volume is reformatted or remounted with a
/// different filesystem. The path may name an existing file or
/// directory; the platform resolves it to the enclosing volume. The
/// input must be valid UTF-8. Relative paths are interpreted against
/// the current working directory where the platform supports them.
///
/// Platform units differ by design and are reported verbatim: Linux and
/// macOS report POSIX _PC_NAME_MAX in bytes, while Windows reports the
/// documented GetVolumeInformationW MaximumComponentLength record in
/// UTF-16 code units. A POSIX platform that defines no fixed bound
/// reports it through the explicit indeterminate flag.
///
/// @param path An existing file or directory path in UTF-8.
/// @return A recorded bound, invalid_argument for an empty path or one
/// containing an embedded null character, invalid_encoding when the path
/// is not valid UTF-8, not_supported when the platform exposes no
/// acceptable source, malformed_data for a determinate bound of zero or
/// another unusable platform record, or a native error reported by the
/// platform's limit interface. Whether the interface validates the whole
/// path before answering follows each platform's implementation.
inline result<path_length_limit> max_component_length(
    const std::string& path) {
    const result<void> valid =
        detail::filesystem_common::validate_path_input(path);
    if (!valid) { return fail(valid.error()); }
    const result<detail::filesystem_common::path_length_snapshot> bound =
        detail::filesystem_common::validate_path_length(
            detail::filesystem_backend::max_component_length(path));
    if (!bound) { return fail(bound.error()); }
    return path_length_limit{bound->length, bound->indeterminate};
}

/// Returns the platform's recorded maximum length of a complete path on
/// the volume containing the given path.
///
/// The bound is a property of the volume and changes only when the
/// volume is reformatted or remounted with a different filesystem. The
/// path may name an existing file or directory; the platform resolves
/// it to the enclosing volume. The input must be valid UTF-8. Relative
/// paths are interpreted against the current working directory where
/// the platform supports them.
///
/// Linux and macOS report the POSIX _PC_PATH_MAX record in bytes;
/// whether the platform's own convention counts a terminating null byte
/// follows that platform's limit documentation and is not normalized.
/// A POSIX platform that defines no fixed bound reports it through the
/// explicit indeterminate flag. Windows reports not_supported because
/// the platform bounds complete paths by process-wide activation policy
/// rather than by any per-volume fact exposed through a public
/// documented interface.
///
/// @param path An existing file or directory path in UTF-8.
/// @return A recorded bound, invalid_argument for an empty path or one
/// containing an embedded null character, invalid_encoding when the path
/// is not valid UTF-8, not_supported when the platform exposes no
/// acceptable source, malformed_data for a determinate bound of zero or
/// another unusable platform record, or a native error reported by the
/// platform's limit interface. Whether the interface validates the whole
/// path before answering follows each platform's implementation; some
/// platforms answer this limit from a recorded constant without touching
/// the path.
inline result<path_length_limit> max_path_length(const std::string& path) {
    const result<void> valid =
        detail::filesystem_common::validate_path_input(path);
    if (!valid) { return fail(valid.error()); }
    const result<detail::filesystem_common::path_length_snapshot> bound =
        detail::filesystem_common::validate_path_length(
            detail::filesystem_backend::max_path_length(path));
    if (!bound) { return fail(bound.error()); }
    return path_length_limit{bound->length, bound->indeterminate};
}

/// Returns the opaque platform-recorded identifier of the volume
/// containing the given path, rendered as fixed-width lowercase
/// hexadecimal digits.
///
/// The identifier is the platform's own recorded volume or filesystem
/// word pair, reported verbatim rather than normalized: Linux renders
/// kernel-documented statfs f_fsid words and macOS renders the statfs
/// f_fsid words as sixteen hexadecimal digits total in recorded order,
/// while Windows renders the documented GetVolumeInformationW serial
/// number as eight hexadecimal digits. An all-zero rendering is
/// valid data because platforms that define no distinguishing
/// identifier for a filesystem record zeros.
///
/// The identifier distinguishes volume instances within one platform.
/// It is not a permanent identity guarantee: reformatting, remounting,
/// and platform-specific identifier reuse change it. Treat the value as
/// potentially identifying and do not log or persist it without an
/// explicit reason.
///
/// The path may name an existing file or directory; the platform
/// resolves it to the enclosing volume. The input must be valid UTF-8.
/// Relative paths are interpreted against the current working directory
/// where the platform supports them.
///
/// @param path An existing file or directory path in UTF-8.
/// @return The fixed-width hexadecimal rendering, invalid_argument for
/// an empty path or one containing an embedded null character,
/// invalid_encoding when the path is not valid UTF-8, not_supported
/// when the platform exposes no acceptable source, malformed_data for
/// an unusable platform record, or a native platform error such as a
/// missing path or denied permission.
inline result<std::string> volume_id(const std::string& path) {
    const result<void> valid =
        detail::filesystem_common::validate_path_input(path);
    if (!valid) { return fail(valid.error()); }
    return detail::filesystem_common::validate_volume_id(
        detail::filesystem_backend::volume_id(path));
}

} // namespace filesystem
} // namespace syscape

#endif
