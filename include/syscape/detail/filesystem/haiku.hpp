#ifndef SYSCAPE_DETAIL_FILESYSTEM_HAIKU_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_HAIKU_HPP

#include <cerrno>
#include <cstdint>
#include <string>
#include <sys/statvfs.h>
#include <system_error>
#include <vector>

#if defined(__has_include)
#if __has_include(<fs_info.h>)
#include <fs_info.h>
#define SYSCAPE_HAS_FS_INFO_H 1
#elif __has_include(<kernel/fs_info.h>)
#include <kernel/fs_info.h>
#define SYSCAPE_HAS_FS_INFO_H 1
#endif
#endif

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
}

inline result<filesystem_common::space_snapshot>
space(const std::string& path) {
    return statvfs_space(path);
}

inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    return pathconf_limit(path, _PC_NAME_MAX);
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& path) {
    return pathconf_limit(path, _PC_PATH_MAX);
}

inline result<std::string> volume_id(const std::string& path) {
#if defined(SYSCAPE_HAS_FS_INFO_H)
    errno = 0;
    const dev_t dev = ::dev_for_path(path.c_str());
    if (dev < 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (errno != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return fail(errc::not_found);
    }
    errno = 0;
    ::fs_info fsi {};
    if (::fs_stat_dev(dev, &fsi) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (errno != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    std::string rendered =
        filesystem_common::render_hex32(static_cast<std::uint32_t>(fsi.dev));
    const auto high =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(fsi.root) >> 32U);
    const auto low =
        static_cast<std::uint32_t>(static_cast<std::uint64_t>(fsi.root) &
                                   static_cast<std::uint64_t>(0xFFFFFFFFU));
    rendered += filesystem_common::render_hex32(high);
    rendered += filesystem_common::render_hex32(low);
    return rendered;
#else
    (void)path;
    return fail(errc::not_supported);
#endif
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
