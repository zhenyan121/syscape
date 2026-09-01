#ifndef SYSCAPE_DETAIL_HARDWARE_ANDROID_HPP
#define SYSCAPE_DETAIL_HARDWARE_ANDROID_HPP

#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/android/property.hpp>
#include <syscape/detail/hardware/common.hpp>
#include <syscape/hardware.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> system_manufacturer() {
    const result<std::string> val =
        android::get_property("ro.product.manufacturer");
    if (val && !val->empty()) {
        return val;
    }
    const result<std::string> brand = android::get_property("ro.product.brand");
    if (brand && !brand->empty()) {
        return brand;
    }
    return fail(errc::not_found);
}

inline result<std::string> system_product_name() {
    const result<std::string> val = android::get_property("ro.product.model");
    if (val && !val->empty()) {
        return val;
    }
    const result<std::string> device =
        android::get_property("ro.product.device");
    if (device && !device->empty()) {
        return device;
    }
    return fail(errc::not_found);
}

inline result<std::string> system_product_version() {
    const result<std::string> rev =
        android::get_property("ro.boot.hardware.revision");
    if (rev && !rev->empty()) {
        return rev;
    }
    const result<std::string> brev = android::get_property("ro.boot.revision");
    if (brev && !brev->empty()) {
        return brev;
    }
    const result<std::string> hrev = android::get_property("ro.revision");
    if (hrev && !hrev->empty()) {
        return hrev;
    }
    return fail(errc::not_supported);
}

inline result<std::string> system_serial_number() {
    return fail(errc::not_supported);
}

inline result<std::string> system_sku() {
    return fail(errc::not_supported);
}

inline result<std::string> system_family() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_manufacturer() {
    return fail(errc::not_supported);
}

inline result<std::string> motherboard_product_name() {
    const result<std::string> val = android::get_property("ro.product.board");
    if (val && !val->empty()) {
        return val;
    }
    return fail(errc::not_supported);
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
    return fail(errc::not_supported);
}

inline result<std::string> firmware_version() {
    const result<std::string> bootloader =
        android::get_property("ro.bootloader");
    if (bootloader && !bootloader->empty() && *bootloader != "unknown") {
        return bootloader;
    }
    const result<std::string> boot_bl =
        android::get_property("ro.boot.bootloader");
    if (boot_bl && !boot_bl->empty() && *boot_bl != "unknown") {
        return boot_bl;
    }
    return fail(errc::not_supported);
}

inline result<std::string> firmware_release_date() {
    return fail(errc::not_supported);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    const result<std::string> charac =
        android::get_property("ro.build.characteristics");
    if (charac && !charac->empty()) {
        const std::string_view sv = *charac;
        if (sv.find("tablet") != std::string_view::npos) {
            return hardware_common::chassis_classification::tablet;
        }
        if (sv.find("automotive") != std::string_view::npos) {
            return hardware_common::chassis_classification::embedded_pc;
        }
        if (sv.find("tv") != std::string_view::npos ||
            sv.find("watch") != std::string_view::npos) {
            return hardware_common::chassis_classification::other;
        }
        if (sv.find("handheld") != std::string_view::npos ||
            sv.find("phone") != std::string_view::npos) {
            return hardware_common::chassis_classification::hand_held;
        }
    }
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
