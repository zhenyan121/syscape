#ifndef SYSCAPE_DETAIL_HARDWARE_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_HARDWARE_OPENHARMONY_HPP

#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/hardware/common.hpp>
#include <syscape/detail/openharmony/parameter.hpp>
#include <syscape/hardware.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace hardware_backend {

inline result<std::string> system_manufacturer() {
    const result<std::string> mfg = openharmony::manufacture();
    if (mfg && !mfg->empty()) {
        return mfg;
    }
    if (!mfg && mfg.error() != errc::not_found &&
        mfg.error() != errc::not_supported) {
        return fail(mfg.error());
    }

    const result<std::string> val =
        openharmony::get_parameter("const.product.manufacturer");
    if (val && !val->empty()) {
        return val;
    }
    if (!val && val.error() != errc::not_found &&
        val.error() != errc::not_supported) {
        return fail(val.error());
    }

    const result<std::string> b = openharmony::brand();
    if (b && !b->empty()) {
        return b;
    }
    if (!b && b.error() != errc::not_found &&
        b.error() != errc::not_supported) {
        return fail(b.error());
    }

    const result<std::string> brand_param =
        openharmony::get_parameter("const.product.brand");
    if (brand_param && !brand_param->empty()) {
        return brand_param;
    }
    if (!brand_param && brand_param.error() != errc::not_found &&
        brand_param.error() != errc::not_supported) {
        return fail(brand_param.error());
    }

    return fail(errc::not_found);
}

inline result<std::string> system_product_name() {
    const result<std::string> model = openharmony::product_model();
    if (model && !model->empty()) {
        return model;
    }
    if (!model && model.error() != errc::not_found &&
        model.error() != errc::not_supported) {
        return fail(model.error());
    }

    const result<std::string> val =
        openharmony::get_parameter("const.product.model");
    if (val && !val->empty()) {
        return val;
    }
    if (!val && val.error() != errc::not_found &&
        val.error() != errc::not_supported) {
        return fail(val.error());
    }

    const result<std::string> name =
        openharmony::get_parameter("const.product.name");
    if (name && !name->empty()) {
        return name;
    }
    if (!name && name.error() != errc::not_found &&
        name.error() != errc::not_supported) {
        return fail(name.error());
    }

    const result<std::string> device =
        openharmony::get_parameter("const.product.device");
    if (device && !device->empty()) {
        return device;
    }
    if (!device && device.error() != errc::not_found &&
        device.error() != errc::not_supported) {
        return fail(device.error());
    }

    return fail(errc::not_found);
}

inline result<std::string> system_product_version() {
    const result<std::string> rev =
        openharmony::get_parameter("const.product.software.version");
    if (rev && !rev->empty()) {
        return rev;
    }
    if (!rev && rev.error() != errc::not_found &&
        rev.error() != errc::not_supported) {
        return fail(rev.error());
    }

    const result<std::string> hrev = openharmony::get_parameter("ro.revision");
    if (hrev && !hrev->empty()) {
        return hrev;
    }
    if (!hrev && hrev.error() != errc::not_found &&
        hrev.error() != errc::not_supported) {
        return fail(hrev.error());
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
    const result<std::string> val =
        openharmony::get_parameter("const.product.board");
    if (val && !val->empty()) {
        return val;
    }
    if (!val && val.error() != errc::not_found &&
        val.error() != errc::not_supported) {
        return fail(val.error());
    }

    const result<std::string> bld =
        openharmony::get_parameter("const.build.product");
    if (bld && !bld->empty()) {
        return bld;
    }
    if (!bld && bld.error() != errc::not_found &&
        bld.error() != errc::not_supported) {
        return fail(bld.error());
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
    const result<std::string> bl =
        openharmony::get_parameter("const.product.bootloader.version");
    if (bl && !bl->empty() && *bl != "unknown") {
        return bl;
    }
    if (!bl && bl.error() != errc::not_found &&
        bl.error() != errc::not_supported) {
        return fail(bl.error());
    }

    const result<std::string> bootloader =
        openharmony::get_parameter("ro.bootloader");
    if (bootloader && !bootloader->empty() && *bootloader != "unknown") {
        return bootloader;
    }
    if (!bootloader && bootloader.error() != errc::not_found &&
        bootloader.error() != errc::not_supported) {
        return fail(bootloader.error());
    }

    return fail(errc::not_supported);
}

inline result<std::string> firmware_release_date() {
    return fail(errc::not_supported);
}

inline result<hardware_common::chassis_classification> chassis_form_factor() {
    result<std::string> dtype = openharmony::device_type();
    if (!dtype && dtype.error() != errc::not_found &&
        dtype.error() != errc::not_supported) {
        return fail(dtype.error());
    }
    if (!dtype || dtype->empty()) {
        dtype = openharmony::get_parameter("const.build.characteristics");
        if (!dtype && dtype.error() != errc::not_found &&
            dtype.error() != errc::not_supported) {
            return fail(dtype.error());
        }
    }
    if (dtype && !dtype->empty()) {
        const std::string_view sv = *dtype;
        if (sv.find("tablet") != std::string_view::npos) {
            return hardware_common::chassis_classification::tablet;
        }
        if (sv.find("car") != std::string_view::npos ||
            sv.find("automotive") != std::string_view::npos) {
            return hardware_common::chassis_classification::embedded_pc;
        }
        if (sv.find("tv") != std::string_view::npos ||
            sv.find("watch") != std::string_view::npos ||
            sv.find("wearable") != std::string_view::npos) {
            return hardware_common::chassis_classification::other;
        }
        if (sv.find("handheld") != std::string_view::npos ||
            sv.find("phone") != std::string_view::npos ||
            sv.find("default") != std::string_view::npos) {
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
