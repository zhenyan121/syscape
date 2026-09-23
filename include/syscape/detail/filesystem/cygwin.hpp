#ifndef SYSCAPE_DETAIL_FILESYSTEM_CYGWIN_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_CYGWIN_HPP

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
            if (sizeof(vfs.f_fsid) > sizeof(std::uint32_t)) {
                const auto high =
                    static_cast<std::uint32_t>((fsid >> 32U) & 0xFFFFFFFFU);
                const auto low = static_cast<std::uint32_t>(
                    fsid & static_cast<std::uint64_t>(0xFFFFFFFFU));
                return filesystem_common::render_hex_word_pair(high, low);
            }
            return filesystem_common::render_hex32(
                static_cast<std::uint32_t>(fsid));
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
