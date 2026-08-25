#ifndef SYSCAPE_DETAIL_BLUETOOTH_MACOS_HPP
#define SYSCAPE_DETAIL_BLUETOOTH_MACOS_HPP

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/detail/bluetooth/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace bluetooth_backend {

inline result<std::string> cf_string_to_utf8(CFStringRef str) {
    if (str == nullptr) {
        return fail(errc::invalid_argument);
    }
    const CFIndex len = CFStringGetLength(str);
    const CFIndex encoded_size =
        CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8);
    if (encoded_size < 0 ||
        static_cast<unsigned long long>(encoded_size) >=
            static_cast<unsigned long long>((std::numeric_limits<std::size_t>::max)())) {
        return fail(errc::value_too_large);
    }
    std::string buffer(static_cast<std::size_t>(encoded_size), '\0');
    CFIndex used_size = 0;
    const CFIndex converted = CFStringGetBytes(
        str,
        CFRangeMake(0, len),
        kCFStringEncodingUTF8,
        0U,
        false,
        buffer.empty() ? nullptr : reinterpret_cast<UInt8*>(buffer.data()),
        encoded_size,
        &used_size);
    if (converted == len && used_size >= 0 && used_size <= encoded_size) {
        buffer.resize(static_cast<std::size_t>(used_size));
        return buffer;
    }
    return fail(errc::invalid_encoding);
}

inline bluetooth::adapter_bus_type
decode_transport(SInt64 transport) noexcept {
    switch (transport) {
    case 0:
        return bluetooth::adapter_bus_type::usb;
    case 1:
        return bluetooth::adapter_bus_type::uart;
    case 2:
        return bluetooth::adapter_bus_type::pci;
    default:
        return bluetooth::adapter_bus_type::unknown;
    }
}

inline result<bool> populate_adapter_info(
    CFDictionaryRef props, bluetooth::adapter_info& info) {
    const auto addr_ref = static_cast<CFDataRef>(
        CFDictionaryGetValue(props, CFSTR("BluetoothDeviceAddress")));
    if (addr_ref != nullptr && CFGetTypeID(addr_ref) == CFDataGetTypeID()) {
        const CFIndex len = CFDataGetLength(addr_ref);
        if (len != 6) {
            return fail(errc::malformed_data);
        }
        const UInt8* bytes = CFDataGetBytePtr(addr_ref);
        if (bytes == nullptr) {
            return fail(errc::malformed_data);
        }
        info.address = bluetooth_common::format_mac_bytes(bytes, false);
    } else {
        const auto addr_str_ref = static_cast<CFStringRef>(
            CFDictionaryGetValue(props, CFSTR("DeviceAddress")));
        if (addr_str_ref != nullptr) {
            if (CFGetTypeID(addr_str_ref) != CFStringGetTypeID()) {
                return fail(errc::malformed_data);
            }
            auto str = cf_string_to_utf8(addr_str_ref);
            if (!str) {
                return fail(str.error());
            }
            auto norm = bluetooth_common::normalize_mac_address(*str);
            if (!norm) {
                return fail(errc::malformed_data);
            }
            info.address = *norm;
        }
    }

    const CFTypeRef transport_ref =
        CFDictionaryGetValue(props, CFSTR("Transport"));
    if (transport_ref != nullptr) {
        if (CFGetTypeID(transport_ref) == CFNumberGetTypeID()) {
            SInt64 transport = -1;
            if (!CFNumberGetValue(
                    static_cast<CFNumberRef>(transport_ref),
                    kCFNumberSInt64Type,
                    &transport)) {
                return fail(errc::malformed_data);
            }
            info.bus = decode_transport(transport);
        } else if (CFGetTypeID(transport_ref) == CFStringGetTypeID()) {
            auto transport = cf_string_to_utf8(
                static_cast<CFStringRef>(transport_ref));
            if (!transport) {
                return fail(transport.error());
            }
            if (transport->find("USB") != std::string::npos) {
                info.bus = bluetooth::adapter_bus_type::usb;
            } else if (transport->find("PCI") != std::string::npos) {
                info.bus = bluetooth::adapter_bus_type::pci;
            } else if (transport->find("UART") != std::string::npos ||
                       transport->find("Serial") != std::string::npos) {
                info.bus = bluetooth::adapter_bus_type::uart;
            }
        } else {
            return fail(errc::malformed_data);
        }
    }

    const auto manufacturer_ref = static_cast<CFNumberRef>(
        CFDictionaryGetValue(props, CFSTR("Manufacturer")));
    if (manufacturer_ref != nullptr) {
        if (CFGetTypeID(manufacturer_ref) != CFNumberGetTypeID()) {
            return fail(errc::malformed_data);
        }
        int manufacturer = 0;
        if (!CFNumberGetValue(
                manufacturer_ref, kCFNumberIntType, &manufacturer) ||
            manufacturer < 0 || manufacturer > 0xFFFF) {
            return fail(errc::malformed_data);
        }
        info.manufacturer_id = static_cast<std::uint16_t>(manufacturer);
    }

    const CFStringRef power_keys[] = {
        CFSTR("ControllerPowerState"), CFSTR("PowerState")};
    for (CFStringRef key : power_keys) {
        const CFTypeRef power_ref = CFDictionaryGetValue(props, key);
        if (power_ref == nullptr) {
            continue;
        }
        if (CFGetTypeID(power_ref) == CFBooleanGetTypeID()) {
            info.power_state = CFBooleanGetValue(
                                   static_cast<CFBooleanRef>(power_ref))
                                   ? bluetooth::adapter_power_state::on
                                   : bluetooth::adapter_power_state::off;
            break;
        }
        if (CFGetTypeID(power_ref) == CFNumberGetTypeID()) {
            int power = 0;
            if (!CFNumberGetValue(
                    static_cast<CFNumberRef>(power_ref),
                    kCFNumberIntType,
                    &power)) {
                return fail(errc::malformed_data);
            }
            if (power == 0) {
                info.power_state = bluetooth::adapter_power_state::off;
            } else if (power == 1) {
                info.power_state = bluetooth::adapter_power_state::on;
            }
            break;
        }
        return fail(errc::malformed_data);
    }

    return true;
}

inline result<std::vector<bluetooth::adapter_info>> adapters() {
    std::vector<bluetooth::adapter_info> result_list;

    CFMutableDictionaryRef matching = IOServiceMatching("IOBluetoothHCIController");
    if (matching == nullptr) {
        return fail(errc::resource_exhausted);
    }

    io_iterator_t iterator = IO_OBJECT_NULL;
#if defined(kIOMainPortDefault)
    const mach_port_t main_port = kIOMainPortDefault;
#else
    const mach_port_t main_port = kIOMasterPortDefault;
#endif

    kern_return_t kr = IOServiceGetMatchingServices(main_port, matching, &iterator);
    if (kr != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (iterator == IO_OBJECT_NULL) {
        return result_list;
    }

    io_service_t service = IO_OBJECT_NULL;
    std::size_t index = 0;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        bluetooth::adapter_info info;
        info.id = "controller" + std::to_string(index);
        info.name = "Apple Bluetooth Controller";

        CFMutableDictionaryRef props = nullptr;
        const kern_return_t property_result = IORegistryEntryCreateCFProperties(
            service, &props, kCFAllocatorDefault, 0);
        if (property_result != KERN_SUCCESS || props == nullptr) {
            IOObjectRelease(service);
            IOObjectRelease(iterator);
            return fail(errc::io_error);
        }
        auto populated = populate_adapter_info(props, info);
        CFRelease(props);
        if (!populated) {
            IOObjectRelease(service);
            IOObjectRelease(iterator);
            return fail(populated.error());
        }

        IOObjectRelease(service);
        result_list.push_back(std::move(info));
        ++index;
    }

    IOObjectRelease(iterator);
    return result_list;
}

inline result<std::size_t> adapter_count() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    return res->size();
}

inline result<bluetooth::adapter_info> default_adapter() {
    auto res = adapters();
    if (!res) {
        return fail(res.error());
    }
    if (res->empty()) {
        return fail(errc::not_found);
    }
    return (*res)[0];
}

inline result<std::vector<bluetooth::device_info>> paired_devices() {
    // Paired Bluetooth device enumeration on modern macOS requires explicit TCC
    // Bluetooth permissions. To remain non-invasive without prompting, return
    // not_supported or empty list.
    return fail(errc::not_supported);
}

inline result<std::vector<bluetooth::device_info>> connected_devices() {
    return fail(errc::not_supported);
}

} // namespace bluetooth_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_BLUETOOTH_MACOS_HPP
