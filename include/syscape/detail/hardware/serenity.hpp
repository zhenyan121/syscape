#ifndef SYSCAPE_DETAIL_HARDWARE_SERENITY_HPP
#define SYSCAPE_DETAIL_HARDWARE_SERENITY_HPP

#include <string>
#include <vector>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> system_manufacturer() {
    return fail(errc::not_supported);
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
