#ifndef SYSCAPE_DETAIL_HARDWARE_SOLARIS_HPP
#define SYSCAPE_DETAIL_HARDWARE_SOLARIS_HPP

#include <cerrno>
#include <cstddef>
#include <string>
#include <sys/systeminfo.h>
#include <system_error>
#include <vector>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> sysinfo_hw_query(int cmd) {
    std::size_t size = 256U;
    for (int attempts = 0; attempts < 4; ++attempts) {
        std::string buffer(size, '\0');
        const long len = ::sysinfo(cmd, &buffer[0], static_cast<long>(size));
        if (len <= 0) {
            if (errno == EINVAL) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) <= size) {
            while (!buffer.empty() &&
                   (buffer.back() == '\0' || buffer.back() == '\n' ||
                    buffer.back() == '\r')) {
                buffer.pop_back();
            }
            if (buffer.empty()) {
                return fail(errc::not_found);
            }
            return buffer;
        }
        size = static_cast<std::size_t>(len) + 16U;
    }
    return fail(errc::malformed_data);
}

inline result<std::string> system_manufacturer() {
#if defined(SI_HW_PROVIDER)
    return sysinfo_hw_query(SI_HW_PROVIDER);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> system_product_name() {
    return fail(errc::not_supported);
}

inline result<std::string> system_product_version() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_manufacturer() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_product_name() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_version() {
    return fail(errc::not_supported);
}

inline result<std::string> firmware_vendor() {
    return fail(errc::not_supported);
}

inline result<std::string> firmware_version() {
    return fail(errc::not_supported);
}

inline result<std::string> firmware_release_date() {
    return fail(errc::not_supported);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    return fail(errc::not_supported);
}

inline result<std::string> hardware_uuid() {
    return fail(errc::not_supported);
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
