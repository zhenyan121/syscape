#ifndef SYSCAPE_DETAIL_INPUT_COMMON_HPP
#define SYSCAPE_DETAIL_INPUT_COMMON_HPP

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/input.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace input_common {

/// Trims leading and trailing ASCII whitespace.
inline std::string_view trim_whitespace(std::string_view text) noexcept {
    while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ') {
        text.remove_prefix(1U);
    }
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ') {
        text.remove_suffix(1U);
    }
    return text;
}

/// Case-insensitive substring search.
inline bool contains_ignore_case(
    std::string_view text, std::string_view needle) noexcept {
    if (needle.empty()) {
        return true;
    }
    if (text.size() < needle.size()) {
        return false;
    }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool matches = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto a = static_cast<unsigned char>(text[i + j]);
            const auto b = static_cast<unsigned char>(needle[j]);
            if (std::tolower(a) != std::tolower(b)) {
                matches = false;
                break;
            }
        }
        if (matches) {
            return true;
        }
    }
    return false;
}

/// Parses an unsigned 16-bit hexadecimal integer (e.g. "0003", "111e", "ab83").
inline result<std::uint16_t> parse_hex_u16(std::string_view input) noexcept {
    const std::string_view trimmed = trim_whitespace(input);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint16_t value = 0U;
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const auto res = std::from_chars(first, last, value, 16);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Parses an unsigned 32-bit decimal integer with bounds checking.
inline result<std::uint32_t> parse_u32(std::string_view input) noexcept {
    const std::string_view trimmed = trim_whitespace(input);
    if (trimmed.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint32_t value = 0U;
    const char* first = trimmed.data();
    const char* last = first + trimmed.size();
    const auto res = std::from_chars(first, last, value, 10);
    if (res.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (res.ec != std::errc() || res.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Maps standard Linux / hardware bus ID to syscape::input::bus_type.
inline ::syscape::input::bus_type bus_from_numeric_id(std::uint16_t bus_id) noexcept {
    switch (bus_id) {
    case 0x0001U: // BUS_PCI
        return ::syscape::input::bus_type::pci;
    case 0x0002U: // BUS_ISAPNP
    case 0x0010U: // BUS_ISA
    case 0x0011U: // BUS_I8042 (PS/2)
    case 0x0012U: // BUS_XTKBD
        return ::syscape::input::bus_type::isa_serio;
    case 0x0003U: // BUS_USB
        return ::syscape::input::bus_type::usb;
    case 0x0005U: // BUS_BLUETOOTH
        return ::syscape::input::bus_type::bluetooth;
    case 0x0006U: // BUS_VIRTUAL
    case 0x0019U: // BUS_HOST (platform/ACPI buttons)
        return ::syscape::input::bus_type::virtual_bus;
    case 0x0018U: // BUS_I2C
        return ::syscape::input::bus_type::i2c;
    case 0x001CU: // BUS_SPI
    case 0x001DU: // BUS_RMI
        return ::syscape::input::bus_type::unknown;
    default:
        return ::syscape::input::bus_type::unknown;
    }
}

/// Filters a list of input devices by exact device_type.
inline std::vector<::syscape::input::input_device> filter_by_type(
    const std::vector<::syscape::input::input_device>& devices,
    ::syscape::input::device_type target_type) {
    std::vector<::syscape::input::input_device> filtered;
    filtered.reserve(devices.size());
    for (const auto& dev : devices) {
        if (dev.type == target_type) {
            filtered.push_back(dev);
        }
    }
    return filtered;
}

/// Filters touch devices (touchpads, touchscreens, digitizers).
inline std::vector<::syscape::input::input_device> filter_touch_devices(
    const std::vector<::syscape::input::input_device>& devices) {
    std::vector<::syscape::input::input_device> filtered;
    filtered.reserve(devices.size());
    for (const auto& dev : devices) {
        if (dev.type == ::syscape::input::device_type::touchpad ||
            dev.type == ::syscape::input::device_type::touchscreen ||
            dev.type == ::syscape::input::device_type::drawing_tablet) {
            filtered.push_back(dev);
        }
    }
    return filtered;
}

/// Filters game controllers (gamepads, joysticks).
inline std::vector<::syscape::input::input_device> filter_gamepads(
    const std::vector<::syscape::input::input_device>& devices) {
    std::vector<::syscape::input::input_device> filtered;
    filtered.reserve(devices.size());
    for (const auto& dev : devices) {
        if (dev.type == ::syscape::input::device_type::gamepad ||
            dev.type == ::syscape::input::device_type::joystick) {
            filtered.push_back(dev);
        }
    }
    return filtered;
}

} // namespace input_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_INPUT_COMMON_HPP
