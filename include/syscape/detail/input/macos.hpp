#ifndef SYSCAPE_DETAIL_INPUT_MACOS_HPP
#define SYSCAPE_DETAIL_INPUT_MACOS_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDManager.h>
#include <IOKit/hid/IOHIDKeys.h>

#include <syscape/input.hpp>
#include <syscape/detail/input/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace input_backend {

class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) {
            ::CFRelease(value_);
        }
    }
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

inline result<std::string> cf_string_to_utf8(::CFStringRef str) {
    if (str == nullptr) {
        return fail(errc::invalid_argument);
    }
    const CFIndex length = ::CFStringGetLength(str);
    const CFIndex maximum =
        ::CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    if (maximum < 0 || maximum == (std::numeric_limits<CFIndex>::max)()) {
        return fail(errc::value_too_large);
    }
    const CFIndex max_size = maximum + 1;
    std::vector<char> buffer(static_cast<std::size_t>(max_size));
    if (::CFStringGetCString(str, buffer.data(), max_size, kCFStringEncodingUTF8)) {
        return std::string(buffer.data());
    }
    return fail(errc::invalid_encoding);
}

inline result<std::optional<long>> get_device_property_long(
    ::IOHIDDeviceRef dev, ::CFStringRef key) {
    const auto prop = ::IOHIDDeviceGetProperty(dev, key);
    if (prop == nullptr) {
        return std::optional<long>{};
    }
    if (::CFGetTypeID(prop) != ::CFNumberGetTypeID()) {
        return fail(errc::malformed_data);
    }
    long value = 0;
    if (!::CFNumberGetValue(
            static_cast<::CFNumberRef>(prop), kCFNumberLongType, &value)) {
        return fail(errc::malformed_data);
    }
    return std::optional<long>{value};
}

inline std::error_code error_from_ioreturn(::IOReturn error) noexcept {
    if (error == kIOReturnNotPrivileged
#ifdef kIOReturnNotPermitted
        || error == kIOReturnNotPermitted
#endif
    ) {
        return make_error_code(errc::permission_denied);
    }
    if (error == kIOReturnUnsupported) {
        return make_error_code(errc::not_supported);
    }
    if (error == kIOReturnNoResources) {
        return make_error_code(errc::resource_exhausted);
    }
    if (error == kIOReturnBusy) {
        return make_error_code(errc::temporarily_unavailable);
    }
    if (error == kIOReturnNotFound) {
        return make_error_code(errc::not_found);
    }
    return make_error_code(errc::io_error);
}

inline result<std::vector<::syscape::input::input_device>> enumerate_hid_devices() {
    const ::IOHIDManagerRef manager = ::IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    if (manager == nullptr) {
        return fail(errc::io_error);
    }
    const cf_object owned_manager(manager);

    ::IOHIDManagerSetDeviceMatching(manager, nullptr);
    const IOReturn open_res = ::IOHIDManagerOpen(manager, kIOHIDOptionsTypeNone);
    if (open_res != kIOReturnSuccess) {
        return fail(error_from_ioreturn(open_res));
    }

    const ::CFSetRef device_set = ::IOHIDManagerCopyDevices(manager);
    if (device_set == nullptr) {
        return std::vector<::syscape::input::input_device>{};
    }
    const cf_object owned_set(device_set);

    const CFIndex count = ::CFSetGetCount(device_set);
    if (count <= 0) {
        return std::vector<::syscape::input::input_device>{};
    }

    std::vector<const void*> raw_devices(static_cast<std::size_t>(count));
    ::CFSetGetValues(device_set, raw_devices.data());

    std::vector<::syscape::input::input_device> devices;
    devices.reserve(static_cast<std::size_t>(count));

    for (const void* item : raw_devices) {
        const auto hid_dev = static_cast<::IOHIDDeviceRef>(const_cast<void*>(item));
        if (hid_dev == nullptr) {
            continue;
        }

        ::syscape::input::input_device dev;

        // Product name
        const auto prod_prop = ::IOHIDDeviceGetProperty(hid_dev, CFSTR(kIOHIDProductKey));
        if (prod_prop != nullptr && ::CFGetTypeID(prod_prop) == ::CFStringGetTypeID()) {
            const auto converted =
                cf_string_to_utf8(static_cast<::CFStringRef>(prod_prop));
            if (!converted) {
                return fail(converted.error());
            }
            dev.name = *converted;
        } else if (prod_prop != nullptr) {
            return fail(errc::malformed_data);
        }

        // Serial / Unique ID
        const auto serial_prop = ::IOHIDDeviceGetProperty(hid_dev, CFSTR(kIOHIDSerialNumberKey));
        if (serial_prop != nullptr && ::CFGetTypeID(serial_prop) == ::CFStringGetTypeID()) {
            const auto converted =
                cf_string_to_utf8(static_cast<::CFStringRef>(serial_prop));
            if (!converted) {
                return fail(converted.error());
            }
            dev.unique_id = *converted;
        } else if (serial_prop != nullptr) {
            return fail(errc::malformed_data);
        }

        // Transport
        const auto transport_prop = ::IOHIDDeviceGetProperty(hid_dev, CFSTR(kIOHIDTransportKey));
        if (transport_prop != nullptr && ::CFGetTypeID(transport_prop) == ::CFStringGetTypeID()) {
            const auto converted =
                cf_string_to_utf8(static_cast<::CFStringRef>(transport_prop));
            if (!converted) {
                return fail(converted.error());
            }
            const std::string& transport = *converted;
            if (input_common::contains_ignore_case(transport, "USB")) {
                dev.bus = ::syscape::input::bus_type::usb;
            } else if (input_common::contains_ignore_case(transport, "Bluetooth")) {
                dev.bus = ::syscape::input::bus_type::bluetooth;
            } else if (input_common::contains_ignore_case(transport, "I2C")) {
                dev.bus = ::syscape::input::bus_type::i2c;
            } else if (input_common::contains_ignore_case(transport, "PCI")) {
                dev.bus = ::syscape::input::bus_type::pci;
            }
        } else if (transport_prop != nullptr) {
            return fail(errc::malformed_data);
        }

        // Hardware IDs
        const auto vendor_id = get_device_property_long(hid_dev, CFSTR(kIOHIDVendorIDKey));
        const auto product_id = get_device_property_long(hid_dev, CFSTR(kIOHIDProductIDKey));
        const auto version_num = get_device_property_long(hid_dev, CFSTR(kIOHIDVersionNumberKey));

        if (!vendor_id || !product_id || !version_num) {
            return fail(!vendor_id ? vendor_id.error()
                                   : !product_id ? product_id.error()
                                                 : version_num.error());
        }

        if (vendor_id->has_value() || product_id->has_value()) {
            const auto in_u16_range = [](long value) noexcept {
                return value >= 0L &&
                       static_cast<unsigned long>(value) <=
                           (std::numeric_limits<std::uint16_t>::max)();
            };
            if ((vendor_id->has_value() && !in_u16_range(**vendor_id)) ||
                (product_id->has_value() && !in_u16_range(**product_id)) ||
                (version_num->has_value() && !in_u16_range(**version_num))) {
                return fail(errc::malformed_data);
            }
            ::syscape::input::input_device_id hw_id;
            hw_id.bus = dev.bus;
            hw_id.vendor_id = vendor_id->has_value()
                                  ? static_cast<std::uint16_t>(**vendor_id) : 0U;
            hw_id.product_id = product_id->has_value()
                                   ? static_cast<std::uint16_t>(**product_id) : 0U;
            hw_id.version = version_num->has_value()
                                ? static_cast<std::uint16_t>(**version_num) : 0U;
            dev.hardware_id = hw_id;
        }

        // Usage and UsagePage
        const auto page = get_device_property_long(hid_dev, CFSTR(kIOHIDPrimaryUsagePageKey));
        const auto usage = get_device_property_long(hid_dev, CFSTR(kIOHIDPrimaryUsageKey));

        if (!page || !usage) {
            return fail(!page ? page.error() : usage.error());
        }

        if (page->has_value() && usage->has_value()) {
            if (**page == 0x01) { // Generic Desktop
                if (**usage == 0x06) {
                    dev.type = ::syscape::input::device_type::keyboard;
                } else if (**usage == 0x02) {
                    dev.type = ::syscape::input::device_type::mouse;
                } else if (**usage == 0x04) {
                    dev.type = ::syscape::input::device_type::joystick;
                } else if (**usage == 0x05) {
                    dev.type = ::syscape::input::device_type::gamepad;
                }
            } else if (**page == 0x0D) { // Digitizer
                if (**usage == 0x04) {
                    dev.type = ::syscape::input::device_type::touchscreen;
                } else if (**usage == 0x05) {
                    dev.type = ::syscape::input::device_type::touchpad;
                } else if (**usage == 0x01 || **usage == 0x02) {
                    dev.type = ::syscape::input::device_type::drawing_tablet;
                }
            }
        }

        if (dev.name.empty()) {
            dev.name = "HID Input Device";
        }
        dev.id = std::to_string(reinterpret_cast<std::uintptr_t>(hid_dev));

        devices.push_back(std::move(dev));
    }

    return devices;
}

inline result<std::vector<::syscape::input::input_device>> devices() {
    return enumerate_hid_devices();
}

inline result<std::vector<::syscape::input::input_device>> keyboards() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::keyboard);
}

inline result<std::vector<::syscape::input::input_device>> mice() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_by_type(*all, ::syscape::input::device_type::mouse);
}

inline result<std::vector<::syscape::input::input_device>> touch_devices() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_touch_devices(*all);
}

inline result<std::vector<::syscape::input::input_device>> gamepads() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return input_common::filter_gamepads(*all);
}

inline result<std::size_t> device_count() {
    const auto all = devices();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

} // namespace input_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_INPUT_MACOS_HPP
