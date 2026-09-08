#ifndef SYSCAPE_DETAIL_FILESYSTEM_NUTTX_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_NUTTX_HPP

#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline result<filesystem_common::space_snapshot>
space(const std::string& path) {
    (void)path;
    // NuttX statvfs currently discards the underlying mount flags, so the
    // mandatory read_only field cannot be populated honestly.
    return fail(errc::not_supported);
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
}

inline result<filesystem_common::path_length_snapshot>
nuttx_pathconf_limit(const std::string& path, int resource) {
    errno = 0;
    const long value = ::pathconf(path.c_str(), resource);
    if (value == -1) {
        const int error = errno;
        if (error == 0) {
            filesystem_common::path_length_snapshot result;
            result.indeterminate = true;
            return result;
        }
        if (error == ENOSYS || error == EINVAL) {
            return fail(errc::not_supported);
        }
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(error, std::generic_category()));
    }
    if (value <= 0) {
        return fail(errc::malformed_data);
    }
    filesystem_common::path_length_snapshot result;
    result.length = static_cast<std::uint64_t>(value);
    return result;
}

inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    return nuttx_pathconf_limit(path, _PC_NAME_MAX);
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& path) {
    return nuttx_pathconf_limit(path, _PC_PATH_MAX);
}

inline result<std::string> volume_id(const std::string& path) {
    (void)path;
    return fail(errc::not_supported);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
