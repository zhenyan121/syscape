#ifndef SYSCAPE_DETAIL_STORAGE_ANDROID_HPP
#define SYSCAPE_DETAIL_STORAGE_ANDROID_HPP

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/android/directory.hpp>
#include <syscape/detail/android/file.hpp>
#include <syscape/detail/storage/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace storage_backend {

inline result<std::string> read_link_basename(const std::string& path,
                                              bool& resolved) {
    resolved = false;
    std::vector<char> buffer(256U);
    constexpr std::size_t maximum_size = 4096U;
    for (;;) {
        const ssize_t length =
            ::readlink(path.c_str(), buffer.data(), buffer.size());
        if (length < 0) {
            if (errno == ENOENT) {
                return std::string();
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (errno != ERANGE) {
                return fail(std::error_code(errno, std::generic_category()));
            }
        } else if (static_cast<std::size_t>(length) < buffer.size()) {
            const std::string_view target(buffer.data(),
                                          static_cast<std::size_t>(length));
            const std::size_t base = target.rfind('/');
            resolved = true;
            if (base == std::string_view::npos) {
                return std::string(target);
            }
            return std::string(target.substr(base + 1U));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::vector<storage_common::drive_record>> drives() {
    android::directory_handle dir("/sys/block");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        if (dir.error() == ENOENT) {
            return std::vector<storage_common::drive_record> {};
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<storage_common::drive_record> list;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string name = entry->d_name;

        // Verify that the entry has a hardware backing device (excludes zram,
        // loop, dm, ram)
        const std::string device_path = "/sys/block/" + name + "/device";
        struct stat backing {};
        if (::lstat(device_path.c_str(), &backing) != 0) {
            if (errno == ENOENT) {
                continue;
            }
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }

        const std::string size_path = "/sys/block/" + name + "/size";
        const auto size_content = android::read_text_file(size_path.c_str());
        if (!size_content) {
            if (size_content.error() == errc::permission_denied) {
                return fail(errc::permission_denied);
            }
            if (size_content.error() != errc::not_found) {
                return fail(size_content.error());
            }
            continue;
        }

        std::string_view trimmed = *size_content;
        android::strip_trailing_newlines(trimmed);
        std::uint64_t sectors = 0U;
        const auto r = std::from_chars(
            trimmed.data(), trimmed.data() + trimmed.size(), sectors);
        if (r.ec != std::errc() || r.ptr != trimmed.data() + trimmed.size()) {
            return fail(errc::malformed_data);
        }

        constexpr std::uint64_t max_u64 =
            (std::numeric_limits<std::uint64_t>::max)();
        if (sectors > max_u64 / 512ULL) {
            return fail(errc::value_too_large);
        }

        bool subsys_resolved = false;
        const auto subsys =
            read_link_basename(device_path + "/subsystem", subsys_resolved);
        if (!subsys) {
            return fail(subsys.error());
        }
        if (!subsys_resolved) {
            continue;
        }

        storage_common::bus_classification bus =
            storage_common::bus_classification::unknown;
        if (*subsys == "scsi") {
            bus = storage_common::bus_classification::scsi;
        } else if (*subsys == "nvme") {
            bus = storage_common::bus_classification::nvme;
        } else if (*subsys == "mmc") {
            bus = storage_common::bus_classification::mmc;
        }

        storage_common::drive_record drive {};
        drive.identifier = name;
        drive.has_capacity_bytes = true;
        drive.capacity_bytes = sectors * 512ULL;
        drive.bus = bus;

        const std::string rot_path = "/sys/block/" + name + "/queue/rotational";
        const auto rot_content = android::read_text_file(rot_path.c_str());
        if (rot_content) {
            std::string_view rot_sv = *rot_content;
            android::strip_trailing_newlines(rot_sv);
            if (rot_sv == "1") {
                drive.has_rotational = true;
                drive.rotational = true;
            } else if (rot_sv == "0") {
                drive.has_rotational = true;
                drive.rotational = false;
            } else {
                return fail(errc::malformed_data);
            }
        } else if (rot_content.error() == errc::permission_denied) {
            return fail(errc::permission_denied);
        } else if (rot_content.error() != errc::not_found) {
            return fail(rot_content.error());
        }

        list.push_back(std::move(drive));
    }
    return list;
}

inline result<std::vector<storage_common::partition_record>> partitions() {
    return fail(errc::not_supported);
}

inline result<storage_common::health_record> health(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

inline result<std::vector<storage_common::health_record>> all_drive_health() {
    return fail(errc::not_supported);
}

} // namespace storage_backend
} // namespace detail
} // namespace syscape

#endif
