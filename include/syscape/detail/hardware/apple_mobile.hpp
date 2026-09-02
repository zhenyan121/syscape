#ifndef SYSCAPE_DETAIL_HARDWARE_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_HARDWARE_APPLE_MOBILE_HPP

#include <cerrno>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <sys/sysctl.h>

#include <syscape/detail/config.hpp>
#include <syscape/detail/hardware/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
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
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n')) {
        value.pop_back();
    }
    return value.empty() ? result<std::string>(fail(errc::not_found))
                         : result<std::string>(std::move(value));
}

inline result<std::string> system_manufacturer() {
    return std::string("Apple");
}

inline result<std::string> system_product_name() {
    result<std::string> val = sysctl_string("hw.model");
    if (!val) {
        val = sysctl_string("hw.machine");
    }
    if (!val) {
        return fail(val.error());
    }
    return val;
}

inline result<std::string> system_product_version() {
    return fail(errc::not_supported);
}

inline result<std::string> system_serial_number() {
    return fail(errc::not_supported);
}

inline result<std::string> system_sku() {
    return fail(errc::not_supported);
}

inline result<std::string> system_family() {
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
    return std::string("Mac");
#elif defined(TARGET_OS_VISION) && TARGET_OS_VISION
    return std::string("Vision");
#elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
    return std::string("Watch");
#elif defined(TARGET_OS_TV) && TARGET_OS_TV
    return std::string("Apple TV");
#elif defined(TARGET_OS_IOS) && TARGET_OS_IOS
    return std::string("iPhone/iPad");
#else
    return std::string("Apple Device");
#endif
}

inline result<std::string> motherboard_manufacturer() {
    return std::string("Apple");
}

inline result<std::string> motherboard_product_name() {
    return sysctl_string("hw.model");
}

inline result<std::string> motherboard_version() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_serial_number() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_asset_tag() {
    return fail(errc::not_supported);
}

inline result<std::string> firmware_vendor() {
    return std::string("Apple");
}

inline result<std::string> firmware_version() {
    return fail(errc::not_supported);
}

inline result<std::string> firmware_release_date() {
    return fail(errc::not_supported);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
    return fail(errc::not_supported);
#elif defined(TARGET_OS_VISION) && TARGET_OS_VISION
    return hardware_common::chassis_classification::wearable;
#elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
    return hardware_common::chassis_classification::wearable;
#elif defined(TARGET_OS_TV) && TARGET_OS_TV
    return hardware_common::chassis_classification::set_top_box;
#elif defined(TARGET_OS_IOS) && TARGET_OS_IOS
    return hardware_common::chassis_classification::hand_held;
#else
    return fail(errc::not_supported);
#endif
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
