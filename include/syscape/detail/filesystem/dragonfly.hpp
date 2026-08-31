#ifndef SYSCAPE_DETAIL_FILESYSTEM_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/param.h>
#include <sys/mount.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline std::string copy_fixed_field(const char* field, std::size_t capacity) {
    return std::string(field, ::strnlen(field, capacity));
}

inline result<filesystem_common::mount_record>
convert_statfs_entry(const struct ::statfs& entry) {
    filesystem_common::mount_record record;
    record.source =
        copy_fixed_field(entry.f_mntfromname, sizeof(entry.f_mntfromname));
    record.mount_point =
        copy_fixed_field(entry.f_mntonname, sizeof(entry.f_mntonname));
    record.file_system_type =
        copy_fixed_field(entry.f_fstypename, sizeof(entry.f_fstypename));
    return record;
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    constexpr int maximum_attempts = 16;

    const int initial_count = ::getfsstat(nullptr, 0, MNT_NOWAIT);
    if (initial_count < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    std::size_t expected = static_cast<std::size_t>(initial_count) + 1U;
    const std::size_t maximum_entries =
        static_cast<std::size_t>((std::numeric_limits<long>::max)()) /
        sizeof(struct ::statfs);
    if (expected > maximum_entries) {
        return fail(errc::value_too_large);
    }

    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::vector<struct ::statfs> entries(expected);
        const std::size_t requested_bytes =
            entries.size() * sizeof(struct ::statfs);
        const int filled = ::getfsstat(
            entries.data(), static_cast<long>(requested_bytes), MNT_NOWAIT);
        if (filled < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        if (static_cast<std::size_t>(filled) >= entries.size()) {
            if (entries.size() > maximum_entries / 2U) {
                return fail(errc::value_too_large);
            }
            expected = entries.size() * 2U;
            continue;
        }

        std::vector<filesystem_common::mount_record> records;
        records.reserve(static_cast<std::size_t>(filled));
        for (int index = 0; index < filled; ++index) {
            result<filesystem_common::mount_record> record =
                convert_statfs_entry(entries[static_cast<std::size_t>(index)]);
            if (!record) {
                return fail(record.error());
            }
            records.push_back(std::move(*record));
        }
        return records;
    }
    return fail(errc::temporarily_unavailable);
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

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
