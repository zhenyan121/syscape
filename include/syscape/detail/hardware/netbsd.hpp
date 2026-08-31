#ifndef SYSCAPE_DETAIL_HARDWARE_NETBSD_HPP
#define SYSCAPE_DETAIL_HARDWARE_NETBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
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

inline result<std::string> read_mib_string(const int* mib,
                                           unsigned int mib_len) {
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
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
            if (errno == ENOMEM) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size > value.size()) {
            continue;
        }
        value.resize(size);
        while (!value.empty() &&
               (value.back() == '\0' || value.back() == '\n' ||
                value.back() == '\r' || value.back() == ' ')) {
            value.pop_back();
        }
        if (!is_valid_utf8(value)) {
            return fail(errc::invalid_encoding);
        }
        return value.empty() ? result<std::string>(fail(errc::not_found))
                             : result<std::string>(std::move(value));
    }
    return fail(errc::temporarily_unavailable);
}

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

inline result<std::string> system_manufacturer() {
    auto res = read_sysctl_by_name("machdep.dmi.system-vendor");
    if (res) {
        return res;
    }
#ifdef HW_VENDOR
    int mib[] = {CTL_HW, HW_VENDOR};
    return read_mib_string(mib, 2U);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> system_product_name() {
    auto res = read_sysctl_by_name("machdep.dmi.system-product");
    if (res) {
        return res;
    }
#ifdef HW_PRODUCT
    int mib[] = {CTL_HW, HW_PRODUCT};
    auto prod = read_mib_string(mib, 2U);
    if (prod) {
        return prod;
    }
#endif
    int mib_model[] = {CTL_HW, HW_MODEL};
    return read_mib_string(mib_model, 2U);
}

inline result<std::string> system_product_version() {
    auto res = read_sysctl_by_name("machdep.dmi.system-version");
    if (res) {
        return res;
    }
#ifdef HW_VERSION
    int mib[] = {CTL_HW, HW_VERSION};
    return read_mib_string(mib, 2U);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> motherboard_manufacturer() {
    return read_sysctl_by_name("machdep.dmi.board-vendor");
}

inline result<std::string> motherboard_product_name() {
    return read_sysctl_by_name("machdep.dmi.board-product");
}

inline result<std::string> motherboard_version() {
    return read_sysctl_by_name("machdep.dmi.board-version");
}

inline result<std::string> firmware_vendor() {
    return read_sysctl_by_name("machdep.dmi.bios-vendor");
}

inline result<std::string> firmware_version() {
    return read_sysctl_by_name("machdep.dmi.bios-version");
}

inline result<std::string> firmware_release_date() {
    return read_sysctl_by_name("machdep.dmi.bios-date");
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    return fail(errc::not_supported);
}

inline result<std::string> hardware_uuid() {
    auto res = read_sysctl_by_name("machdep.dmi.system-uuid");
    if (res) {
        return res;
    }
#ifdef HW_UUID
    int mib[] = {CTL_HW, HW_UUID};
    return read_mib_string(mib, 2U);
#else
    return fail(errc::not_supported);
#endif
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
