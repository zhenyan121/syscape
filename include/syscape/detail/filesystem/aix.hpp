#ifndef SYSCAPE_DETAIL_FILESYSTEM_AIX_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_AIX_HPP

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>

#if defined(__has_include)
#if __has_include(<sys/mntctl.h>)
#include <sys/mntctl.h>
#define SYSCAPE_HAS_AIX_MNTCTL 1
#endif
#if __has_include(<sys/vmount.h>)
#include <sys/vmount.h>
#define SYSCAPE_HAS_AIX_VMOUNT 1
#endif
#if __has_include(<sys/vfs.h>)
#include <sys/vfs.h>
#define SYSCAPE_HAS_AIX_VFS 1
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
#if defined(SYSCAPE_HAS_AIX_MNTCTL) && defined(SYSCAPE_HAS_AIX_VMOUNT)
    int size = 0;
    if (::mntctl(MCTL_QUERY, sizeof(int), reinterpret_cast<char*>(&size)) !=
        0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size <= 0) {
        return fail(errc::malformed_data);
    }

    std::vector<char> buffer(static_cast<std::size_t>(size));
    const int count = ::mntctl(MCTL_QUERY, size, buffer.data());
    if (count < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<filesystem_common::mount_record> records;
    records.reserve(static_cast<std::size_t>(count));

    char* ptr = buffer.data();
    for (int i = 0; i < count; ++i) {
        const auto* vmt = reinterpret_cast<const struct ::vmount*>(ptr);
        filesystem_common::mount_record rec {};

        const char* obj = reinterpret_cast<const char*>(vmt) +
                          vmt->vmt_data[VMT_OBJECT].vmt_off;
        if (obj != nullptr && vmt->vmt_data[VMT_OBJECT].vmt_size > 0) {
            rec.source = std::string(obj, vmt->vmt_data[VMT_OBJECT].vmt_size);
            while (!rec.source.empty() && rec.source.back() == '\0') {
                rec.source.pop_back();
            }
        }

        const char* stub = reinterpret_cast<const char*>(vmt) +
                           vmt->vmt_data[VMT_STUB].vmt_off;
        if (stub != nullptr && vmt->vmt_data[VMT_STUB].vmt_size > 0) {
            rec.mount_point =
                std::string(stub, vmt->vmt_data[VMT_STUB].vmt_size);
            while (!rec.mount_point.empty() && rec.mount_point.back() == '\0') {
                rec.mount_point.pop_back();
            }
        }

#if defined(SYSCAPE_HAS_AIX_VFS)
        const struct ::vfs_ent* vfe = ::getvfsbytype(vmt->vmt_gfstype);
        if (vfe != nullptr && vfe->vfsent_name != nullptr) {
            rec.file_system_type = vfe->vfsent_name;
        }
#endif
        if (rec.file_system_type.empty()) {
            rec.file_system_type = "unknown";
        }

        records.push_back(std::move(rec));
        ptr += vmt->vmt_length;
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
            if (fsid != 0ULL) {
                if (sizeof(vfs.f_fsid) > sizeof(std::uint32_t)) {
                    const auto high =
                        static_cast<std::uint32_t>((fsid >> 32U) & 0xFFFFFFFFU);
                    const auto low =
                        static_cast<std::uint32_t>(fsid & 0xFFFFFFFFU);
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
