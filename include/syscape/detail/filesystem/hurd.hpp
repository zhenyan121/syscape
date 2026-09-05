#ifndef SYSCAPE_DETAIL_FILESYSTEM_HURD_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_HURD_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mntent.h>
#include <mutex>
#include <string>
#include <string_view>
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
    static std::mutex mnt_mutex;
    std::lock_guard<std::mutex> lock(mnt_mutex);

    FILE* fp = ::setmntent("/proc/mounts", "r");
    if (fp == nullptr) {
        fp = ::setmntent("/etc/mtab", "r");
    }
    if (fp == nullptr) {
        const int saved_errno = errno;
        if (saved_errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }

    struct mntent_cleanup {
        FILE* handle;
        ~mntent_cleanup() {
            if (handle != nullptr) {
                ::endmntent(handle);
            }
        }
    } cleanup {fp};

    std::vector<filesystem_common::mount_record> records;
    struct ::mntent* entry = nullptr;
    while ((entry = ::getmntent(fp)) != nullptr) {
        if (entry->mnt_dir == nullptr || entry->mnt_dir[0] == '\0') {
            continue;
        }
        filesystem_common::mount_record rec;
        rec.mount_point = std::string(entry->mnt_dir);
        if (entry->mnt_fsname != nullptr && entry->mnt_fsname[0] != '\0') {
            rec.source = std::string(entry->mnt_fsname);
        }
        if (entry->mnt_type != nullptr && entry->mnt_type[0] != '\0') {
            rec.file_system_type = std::string(entry->mnt_type);
        }
        records.push_back(std::move(rec));
    }

    if (::ferror(fp) != 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return records;
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
            if (sizeof(vfs.f_fsid) == sizeof(std::uint32_t)) {
                std::uint32_t raw_word = 0U;
                std::memcpy(&raw_word, &vfs.f_fsid, sizeof(raw_word));
                if (raw_word != 0U) {
                    return filesystem_common::render_hex32(raw_word);
                }
            } else if (sizeof(vfs.f_fsid) >= sizeof(std::uint32_t) * 2U) {
                std::uint32_t words[2] = {0U, 0U};
                std::memcpy(words, &vfs.f_fsid, sizeof(words));
                if (words[0] != 0U || words[1] != 0U) {
                    return filesystem_common::render_hex_word_pair(words[0],
                                                                   words[1]);
                }
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
