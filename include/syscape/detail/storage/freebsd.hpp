#ifndef SYSCAPE_DETAIL_STORAGE_FREEBSD_HPP
#define SYSCAPE_DETAIL_STORAGE_FREEBSD_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/storage.hpp>
#include <syscape/detail/storage/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

inline storage_common::bus_classification
classify_bus_name(std::string_view name) noexcept {
    if (name.rfind("nvme", 0) == 0 || name.rfind("nvd", 0) == 0 ||
        name.rfind("nda", 0) == 0) {
        return storage_common::bus_classification::nvme;
    }
    if (name.rfind("ada", 0) == 0) {
        return storage_common::bus_classification::sata;
    }
    if (name.rfind("da", 0) == 0) {
        return storage_common::bus_classification::scsi;
    }
    if (name.rfind("cd", 0) == 0 || name.rfind("acd", 0) == 0) {
        return storage_common::bus_classification::atapi;
    }
    if (name.rfind("vtbd", 0) == 0) {
        return storage_common::bus_classification::virtual_media;
    }
    return storage_common::bus_classification::unknown;
}

inline result<std::string> read_sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

inline result<std::vector<storage_common::drive_record>> drives() {
    const result<std::string> disks_str = read_sysctl_string("kern.disks");
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
        std::size_t end = start;
        while (end < disks_str->size() && (*disks_str)[end] != ' ') {
            ++end;
        }
        const std::string name = disks_str->substr(start, end - start);
        start = end;

        storage_common::drive_record rec;
        rec.identifier = name;
        rec.bus = classify_bus_name(name);

        const std::string dev_path = "/dev/" + name;
        const int fd = ::open(dev_path.c_str(), O_RDONLY | O_CLOEXEC);
        if (fd >= 0) {
            off_t mediasize = 0;
            if (::ioctl(fd, DIOCGMEDIASIZE, &mediasize) == 0 && mediasize > 0) {
                rec.has_capacity_bytes = true;
                rec.capacity_bytes = static_cast<std::uint64_t>(mediasize);
            }
            unsigned int sectorsize = 0;
            if (::ioctl(fd, DIOCGSECTORSIZE, &sectorsize) == 0 &&
                sectorsize > 0) {
                rec.has_logical_sector_size_bytes = true;
                rec.logical_sector_size_bytes = sectorsize;
            }
            ::close(fd);
        }

        list.push_back(std::move(rec));
    }

    std::sort(list.begin(), list.end(),
              [](const storage_common::drive_record& a,
                 const storage_common::drive_record& b) noexcept {
                  return a.identifier < b.identifier;
              });

    return list;
}

inline result<std::vector<storage_common::partition_record>> partitions() {
    return std::vector<storage_common::partition_record>();
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
