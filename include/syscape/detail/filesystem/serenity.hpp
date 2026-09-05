#ifndef SYSCAPE_DETAIL_FILESYSTEM_SERENITY_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_SERENITY_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline result<filesystem_common::space_snapshot>
space(const std::string& path) {
    return statvfs_space(path);
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
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
    for (;;) {
        struct ::statvfs vfs {};
        if (::statvfs(path.c_str(), &vfs) == 0) {
            const auto fsid = static_cast<std::uint64_t>(vfs.f_fsid);
            if (fsid != 0U) {
                if (sizeof(vfs.f_fsid) > sizeof(std::uint32_t)) {
                    const auto high = static_cast<std::uint32_t>(fsid >> 32U);
                    const auto low = static_cast<std::uint32_t>(
                        fsid & static_cast<std::uint64_t>(0xFFFFFFFFU));
                    return filesystem_common::render_hex_word_pair(high, low);
                }
                return filesystem_common::render_hex32(
                    static_cast<std::uint32_t>(fsid));
            }
        } else if (errno != EINTR) {
            const int err = errno;
            if (err == ENOENT || err == ENOTDIR) {
                return fail(errc::not_found);
            }
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(err, std::generic_category()));
        } else {
            continue;
        }

        struct ::stat st {};
        if (::stat(path.c_str(), &st) == 0) {
            const auto dev = static_cast<std::uint64_t>(st.st_dev);
            const auto high = static_cast<std::uint32_t>(dev >> 32U);
            const auto low = static_cast<std::uint32_t>(
                dev & static_cast<std::uint64_t>(0xFFFFFFFFU));
            return filesystem_common::render_hex_word_pair(high, low);
        }
        if (errno != EINTR) {
            const int err = errno;
            if (err == ENOENT || err == ENOTDIR) {
                return fail(errc::not_found);
            }
            if (err == EACCES || err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(err, std::generic_category()));
        }
    }
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
