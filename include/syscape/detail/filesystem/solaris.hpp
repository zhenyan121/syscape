#ifndef SYSCAPE_DETAIL_FILESYSTEM_SOLARIS_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_SOLARIS_HPP

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>
#include <sys/statvfs.h>
#include <sys/types.h>

#if defined(__has_include)
#if __has_include(<sys/mnttab.h>)
#include <sys/mnttab.h>
#define SYSCAPE_HAS_MNTTAB 1
#endif
#endif

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
#if defined(SYSCAPE_HAS_MNTTAB)
    FILE* fp = std::fopen("/etc/mnttab", "r");
    if (fp == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    struct file_guard {
        FILE* f;
        ~file_guard() {
            if (f != nullptr) {
                std::fclose(f);
            }
        }
    } guard {fp};

    std::vector<filesystem_common::mount_record> records;
    struct ::mnttab mp {};

    int status = 0;
    while ((status = ::getmntent(fp, &mp)) == 0) {
        if (mp.mnt_mountp == nullptr || mp.mnt_mountp[0] == '\0') {
            continue;
        }

        filesystem_common::mount_record rec {};
        rec.mount_point = std::string(mp.mnt_mountp);
        if (mp.mnt_special != nullptr) {
            rec.source = std::string(mp.mnt_special);
        }
        if (mp.mnt_fstype != nullptr) {
            rec.file_system_type = std::string(mp.mnt_fstype);
        }

        records.push_back(std::move(rec));
    }

    if (status != -1) {
        return fail(errc::malformed_data);
    }
    if (std::ferror(fp)) {
        return fail(errc::io_error);
    }

    return records;
#else
    return fail(errc::not_supported);
#endif
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
            const std::uint64_t fsid = static_cast<std::uint64_t>(vfs.f_fsid);
            if (sizeof(vfs.f_fsid) > sizeof(std::uint32_t)) {
                const std::uint32_t first =
                    static_cast<std::uint32_t>((fsid >> 32) & 0xFFFFFFFFU);
                const std::uint32_t second =
                    static_cast<std::uint32_t>(fsid & 0xFFFFFFFFU);
                return filesystem_common::render_hex_word_pair(first, second);
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
