#ifndef SYSCAPE_DETAIL_FILESYSTEM_POSIX_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_POSIX_HPP

#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>

#include <sys/statvfs.h>
#include <unistd.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

/// Scales a block count by its block size with overflow rejection.
///
/// Platform block counts and sizes are converted to their unsigned 64-bit
/// magnitudes before this helper, so every product either fits comfortably
/// or reports value_too_large instead of wrapping.
inline result<std::uint64_t> scaled_block_count(
    std::uint64_t blocks, std::uint64_t block_size) {
    constexpr std::uint64_t maximum =
        (std::numeric_limits<std::uint64_t>::max)();
    if (block_size != 0U && blocks > maximum / block_size) {
        return fail(errc::value_too_large);
    }
    return blocks * block_size;
}

/// Converts a filled POSIX statvfs record into portable capacity values.
///
/// The fundamental block size follows the POSIX rule of using f_frsize,
/// falling back to f_bsize where the platform records zero. A volume that
/// exposes no nonzero block size cannot express byte counts and is rejected
/// as malformed platform data. The read-only state comes from the
/// documented ST_RDONLY status flag, which every conforming statvfs
/// implementation defines.
inline result<filesystem_common::space_snapshot> compute_space_snapshot(
    const struct ::statvfs& status) {
    filesystem_common::space_snapshot snapshot;

    const std::uint64_t frsize = static_cast<std::uint64_t>(status.f_frsize);
    const std::uint64_t bsize = static_cast<std::uint64_t>(status.f_bsize);
    snapshot.block_size_bytes = frsize != 0U ? frsize : bsize;
    if (snapshot.block_size_bytes == 0U) {
        return fail(errc::malformed_data);
    }

    const std::uint64_t blocks = static_cast<std::uint64_t>(status.f_blocks);
    const std::uint64_t free_blocks = static_cast<std::uint64_t>(status.f_bfree);
    const std::uint64_t available_blocks =
        static_cast<std::uint64_t>(status.f_bavail);

    result<std::uint64_t> capacity =
        scaled_block_count(blocks, snapshot.block_size_bytes);
    if (!capacity) { return fail(capacity.error()); }
    result<std::uint64_t> free_bytes =
        scaled_block_count(free_blocks, snapshot.block_size_bytes);
    if (!free_bytes) { return fail(free_bytes.error()); }
    result<std::uint64_t> available =
        scaled_block_count(available_blocks, snapshot.block_size_bytes);
    if (!available) { return fail(available.error()); }

    snapshot.capacity_bytes = *capacity;
    snapshot.free_bytes = *free_bytes;
    snapshot.available_bytes = *available;
    snapshot.read_only = (status.f_flag & ST_RDONLY) != 0U;
    return snapshot;
}

/// Queries capacity through POSIX statvfs with interrupted-call retries.
inline result<filesystem_common::space_snapshot> statvfs_space(
    const std::string& path) {
    for (;;) {
        struct ::statvfs status {};
        if (::statvfs(path.c_str(), &status) == 0) {
            return compute_space_snapshot(status);
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

/// Converts one raw pathconf outcome into a portable length bound.
///
/// POSIX reports a failing query as -1 with errno set and a limit with
/// no fixed value as -1 with errno unchanged, so the return value and
/// the saved errno must be examined together. A value below -1 violates
/// the documented contract and a determinate bound of zero cannot name
/// even one path component; both are malformed platform data rather
/// than plausible values.
inline result<filesystem_common::path_length_snapshot>
convert_pathconf_outcome(long value, int saved_errno) {
    if (value == -1) {
        if (saved_errno == 0) {
            filesystem_common::path_length_snapshot indeterminate;
            indeterminate.indeterminate = true;
            return indeterminate;
        }
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    if (value <= 0) {
        return fail(errc::malformed_data);
    }
    filesystem_common::path_length_snapshot snapshot;
    snapshot.length = static_cast<std::uint64_t>(value);
    return snapshot;
}

/// Queries one documented pathconf limit for the volume holding path.
///
/// errno is cleared before the call because POSIX distinguishes a
/// failing query from an indeterminate limit only by whether the call
/// stored an error code.
inline result<filesystem_common::path_length_snapshot> pathconf_limit(
    const std::string& path, int resource) {
    errno = 0;
    const long value = ::pathconf(path.c_str(), resource);
    return convert_pathconf_outcome(value, value == -1 ? errno : 0);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
