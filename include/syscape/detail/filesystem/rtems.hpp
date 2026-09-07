#ifndef SYSCAPE_DETAIL_FILESYSTEM_RTEMS_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_RTEMS_HPP

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
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline result<filesystem_common::space_snapshot>
space(const std::string& path) {
    (void)path;
    // RTEMS statvfs exposes no documented, portable flag that can populate
    // the read_only member of the complete space snapshot.
    return fail(errc::not_supported);
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    return fail(errc::not_supported);
}

inline result<filesystem_common::path_length_snapshot>
rtems_pathconf_limit(const std::string& path, int resource) {
    errno = 0;
    const long value = ::pathconf(path.c_str(), resource);
    if (value == -1) {
        const int error = errno;
        if (error == 0) {
            filesystem_common::path_length_snapshot indeterminate;
            indeterminate.indeterminate = true;
            return indeterminate;
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
    filesystem_common::path_length_snapshot snapshot;
    snapshot.length = static_cast<std::uint64_t>(value);
    return snapshot;
}

inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    return rtems_pathconf_limit(path, _PC_NAME_MAX);
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& path) {
    return rtems_pathconf_limit(path, _PC_PATH_MAX);
}

inline result<std::string> volume_id(const std::string& path) {
    struct ::statvfs vfs {};
    for (;;) {
        if (::statvfs(path.c_str(), &vfs) == 0) {
            break;
        }
        const int error = errno;
        if (error == EINTR) {
            continue;
        }
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(error, std::generic_category()));
    }

    // RTEMS declares statvfs::f_fsid as unsigned long. A zero value means
    // that this filesystem did not supply an identifier, so use st_dev as the
    // documented filesystem-device fallback.
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

    for (;;) {
        struct ::stat st {};
        if (::stat(path.c_str(), &st) == 0) {
            const auto dev = static_cast<std::uint64_t>(st.st_dev);
            const auto high = static_cast<std::uint32_t>(dev >> 32U);
            const auto low = static_cast<std::uint32_t>(
                dev & static_cast<std::uint64_t>(0xFFFFFFFFU));
            return filesystem_common::render_hex_word_pair(high, low);
        }
        const int error = errno;
        if (error == EINTR) {
            continue;
        }
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(error, std::generic_category()));
    }
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
