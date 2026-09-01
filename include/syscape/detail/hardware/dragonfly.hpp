#ifndef SYSCAPE_DETAIL_HARDWARE_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_HARDWARE_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <kenv.h>
#include <string>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/hardware.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> read_sysctl_by_name(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    value.resize(size);
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r' || value.back() == ' ')) {
        value.pop_back();
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    return value.empty() ? result<std::string>(fail(errc::not_found))
                         : result<std::string>(std::move(value));
}

inline result<std::string> read_kenv_value(const char* name) {
    char buffer[256] = {};
    const int ret = ::kenv(KENV_GET, name, buffer, sizeof(buffer));
    if (ret < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    std::string value(buffer);
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r' || value.back() == ' ')) {
        value.pop_back();
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    if (value.empty()) {
        return fail(errc::not_found);
    }
    return value;
}

inline result<std::string> system_manufacturer() {
    auto res = read_sysctl_by_name("machdep.dmi.system-vendor");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.system.maker");
}

inline result<std::string> system_product_name() {
    auto res = read_sysctl_by_name("machdep.dmi.system-product");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.system.product");
}

inline result<std::string> system_product_version() {
    auto res = read_sysctl_by_name("machdep.dmi.system-version");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.system.version");
}

inline result<std::string> motherboard_manufacturer() {
    auto res = read_sysctl_by_name("machdep.dmi.board-vendor");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.planar.maker");
}

inline result<std::string> motherboard_product_name() {
    auto res = read_sysctl_by_name("machdep.dmi.board-product");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.planar.product");
}

inline result<std::string> motherboard_version() {
    auto res = read_sysctl_by_name("machdep.dmi.board-version");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.planar.version");
}

inline result<std::string> firmware_vendor() {
    auto res = read_sysctl_by_name("machdep.dmi.bios-vendor");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.bios.vendor");
}

inline result<std::string> firmware_version() {
    auto res = read_sysctl_by_name("machdep.dmi.bios-version");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.bios.version");
}

inline result<std::string> firmware_release_date() {
    auto res = read_sysctl_by_name("machdep.dmi.bios-date");
    if (res) {
        return res;
    }
    return read_kenv_value("smbios.bios.reldate");
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    return fail(errc::not_supported);
}

inline result<std::string> hardware_uuid() {
    auto res = read_sysctl_by_name("machdep.dmi.system-uuid");
    if (res) {
        return res;
    }
    auto hostuuid = read_sysctl_by_name("kern.hostuuid");
    if (hostuuid) {
        return hostuuid;
    }
    auto hwuuid = read_sysctl_by_name("hw.uuid");
    if (hwuuid) {
        return hwuuid;
    }
    return read_kenv_value("smbios.system.uuid");
}

inline result<std::vector<::syscape::hardware::pci_device>> pci_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::hardware::usb_device>> usb_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::hardware::memory_device>>
memory_devices() {
    return fail(errc::not_supported);
}

} // namespace hardware_backend
} // namespace detail
} // namespace syscape

#endif
