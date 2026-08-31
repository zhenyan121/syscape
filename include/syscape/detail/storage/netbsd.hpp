#ifndef SYSCAPE_DETAIL_STORAGE_NETBSD_HPP
#define SYSCAPE_DETAIL_STORAGE_NETBSD_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/storage/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>
#include <syscape/storage.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

inline storage_common::bus_classification
classify_bus_name(std::string_view name) noexcept {
    if (name.rfind("nvme", 0) == 0) {
        return storage_common::bus_classification::nvme;
    }
    if (name.rfind("wd", 0) == 0) {
        return storage_common::bus_classification::sata;
    }
    if (name.rfind("sd", 0) == 0) {
        return storage_common::bus_classification::scsi;
    }
    if (name.rfind("cd", 0) == 0) {
        return storage_common::bus_classification::atapi;
    }
    if (name.rfind("ld", 0) == 0 || name.rfind("vnd", 0) == 0) {
        return storage_common::bus_classification::virtual_media;
    }
    return storage_common::bus_classification::unknown;
}

inline result<std::string> read_mib_string(const int* mib,
                                           unsigned int mib_len) {
    std::size_t size = 0U;
    int mib_copy[8];
    if (mib_len > 8U) {
        return fail(errc::not_supported);
    }
    for (unsigned int i = 0; i < mib_len; ++i) {
        mib_copy[i] = mib[i];
    }
    if (::sysctl(mib_copy, mib_len, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctl(mib_copy, mib_len, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r')) {
        value.pop_back();
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    return value;
}

inline result<std::vector<storage_common::drive_record>> drives() {
#ifdef HW_DISKNAMES
    int mib[] = {CTL_HW, HW_DISKNAMES};
    const result<std::string> disks_str = read_mib_string(mib, 2U);
    if (!disks_str) {
        if (disks_str.error() == errc::not_found) {
            return std::vector<storage_common::drive_record>();
        }
        return fail(disks_str.error());
    }

    std::vector<storage_common::drive_record> list;
    std::size_t start = 0U;
    while (start < disks_str->size()) {
        while (start < disks_str->size() && (*disks_str)[start] == ' ') {
            ++start;
        }
        if (start >= disks_str->size()) {
            break;
        }
        std::size_t space = disks_str->find(' ', start);
        std::string disk_name = (space == std::string::npos)
                                    ? disks_str->substr(start)
                                    : disks_str->substr(start, space - start);
        start = (space == std::string::npos) ? disks_str->size() : space + 1U;

        if (disk_name.empty()) {
            continue;
        }

        storage_common::drive_record rec;
        rec.identifier = disk_name;
        rec.bus = classify_bus_name(disk_name);
        list.push_back(std::move(rec));
    }

    std::sort(list.begin(), list.end(),
              [](const storage_common::drive_record& a,
                 const storage_common::drive_record& b) noexcept {
                  return a.identifier < b.identifier;
              });

    return list;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<storage_common::partition_record>> partitions() {
    return fail(errc::not_supported);
}

inline result<storage_common::health_record> health(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<storage_common::health_record>> all_drive_health() {
    return fail(errc::not_supported);
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
