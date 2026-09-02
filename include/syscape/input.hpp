#ifndef SYSCAPE_INPUT_HPP
#define SYSCAPE_INPUT_HPP

/// @file
/// @brief Hosted input devices, keyboards, pointing devices, touch, and game
/// controllers.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Apple mobile platforms expose no permitted public input-device
/// inventory source to this C++ interface, so all queries report
/// not_supported.
/// @note This module exposes:
/// - Enumeration of input devices (devices(), keyboards(), mice(),
/// touch_devices(), gamepads()).
/// - Total input device count (device_count()).
/// - Device classification (keyboard, mouse, touchpad, touchscreen, joystick,
/// gamepad, tablet, switch).
/// - Hardware bus classification (USB, Bluetooth, I2C, PCI, ISA/serio,
/// virtual).
/// - Vendor, product, version IDs, physical location, sysfs nodes, and handler
/// bindings.
/// @note Linux queries kernel interfaces (/proc/bus/input/devices,
/// /sys/class/input).
/// @note Windows queries Win32 Raw Input interfaces (GetRawInputDeviceList,
/// GetRawInputDeviceInfoW).
/// @note macOS queries Darwin IOKit / IOHIDManager interfaces.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/input.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace input {

/// High-level classification of an input device.
enum class device_type : std::uint8_t {
    /// Device type is unknown or unspecified.
    unknown,
    /// Alphanumeric keyboard or keypad.
    keyboard,
    /// Mouse, trackball, or relative pointing device.
    mouse,
    /// Touchpad / trackpad.
    touchpad,
    /// Touchscreen / digitizer surface.
    touchscreen,
    /// Flight stick, joystick, or directional controller.
    joystick,
    /// Console gamepad or game controller.
    gamepad,
    /// Graphics / drawing tablet or stylus digitizer.
    drawing_tablet,
    /// Power / lid / sleep button, hardware switch, or chassis sensor.
    button_or_switch
};

/// Hardware bus transport type.
enum class bus_type : std::uint8_t {
    /// Bus type is unknown or unspecified.
    unknown,
    /// Universal Serial Bus (USB).
    usb,
    /// Bluetooth wireless bus.
    bluetooth,
    /// PCI / PCIe bus.
    pci,
    /// Inter-Integrated Circuit (I2C) or SMBus.
    i2c,
    /// Legacy ISA / serio (e.g. PS/2 keyboard or mouse).
    isa_serio,
    /// Virtual / software / platform device.
    virtual_bus
};

/// Hardware vendor, product, and version identifiers.
struct input_device_id {
    /// Hardware transport bus.
    bus_type bus = bus_type::unknown;

    /// 16-bit Vendor ID (e.g. USB or PCI vendor ID), if exposed.
    std::uint16_t vendor_id = 0U;

    /// 16-bit Product / Model ID, if exposed.
    std::uint16_t product_id = 0U;

    /// 16-bit Version / Revision number, if exposed.
    std::uint16_t version = 0U;
};

/// Information describing a single connected or integrated input device.
struct input_device {
    /// Platform-specific identifier or path (e.g. "input0", "/dev/input/event3", or raw input handle).
    std::string id;

    /// Human-readable device name (e.g. "AT Translated Set 2 keyboard", "Logitech USB Optical Mouse").
    std::string name;

    /// High-level device classification.
    device_type type = device_type::unknown;

    /// Hardware transport bus.
    bus_type bus = bus_type::unknown;

    /// Hardware vendor, product, and version identifiers if available.
    std::optional<input_device_id> hardware_id;

    /// Physical device topology path (e.g. "usb-0000:05:00.4-1/input0", "isa0060/serio0/input0").
    std::optional<std::string> physical_location;

    /// Sysfs path or device node path, if exposed.
    std::optional<std::string> sysfs_path;

    /// Unique identifier or serial number string if reported by hardware.
    std::optional<std::string> unique_id;

    /// Associated event handler nodes (e.g. ["kbd", "event3"], ["mouse0", "event4"]).
    std::vector<std::string> handlers;

    /// Indicates whether this device is integrated/internal (e.g. laptop keyboard/touchpad) vs external,
    /// if determinable by the platform.
    std::optional<bool> is_integrated;
};

} // namespace input
} // namespace syscape

#include <syscape/detail/input/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/input/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/input/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/input/macos.hpp>
#else
#include <syscape/detail/input/generic.hpp>
#endif

namespace syscape {
namespace input {

/// Enumerates all detected input devices across all device types.
///
/// @return A vector of input_device entries; not_supported when the input subsystem
/// is unavailable; permission_denied when access is denied; malformed_data for
/// invalid platform data; invalid_encoding for failed native text conversion;
/// or a native I/O error.
/// @note The collection can change when devices are plugged, unplugged, or reconfigured.
inline result<std::vector<input_device>> devices() {
    return detail::input_backend::devices();
}

/// Enumerates all detected keyboards and keypads.
///
/// @return A vector of keyboard input_device entries, or the same platform,
/// permission, encoding, malformed-data, and I/O errors as devices().
/// @note The collection can change when devices are plugged, unplugged, or reconfigured.
inline result<std::vector<input_device>> keyboards() {
    return detail::input_backend::keyboards();
}

/// Enumerates all detected mice, trackballs, and relative pointing devices.
///
/// @return A vector of mouse input_device entries, or the same platform,
/// permission, encoding, malformed-data, and I/O errors as devices().
/// @note The collection can change when devices are plugged, unplugged, or reconfigured.
inline result<std::vector<input_device>> mice() {
    return detail::input_backend::mice();
}

/// Enumerates all detected touch devices (touchpads, touchscreens, digitizers).
///
/// @return A vector of touch input_device entries, or the same platform,
/// permission, encoding, malformed-data, and I/O errors as devices().
/// @note The collection can change when devices are plugged, unplugged, or reconfigured.
inline result<std::vector<input_device>> touch_devices() {
    return detail::input_backend::touch_devices();
}

/// Enumerates all detected game controllers, gamepads, and joysticks.
///
/// @return A vector of gamepad and joystick input_device entries, or the same
/// platform, permission, encoding, malformed-data, and I/O errors as devices().
/// @note The collection can change when devices are plugged, unplugged, or reconfigured.
inline result<std::vector<input_device>> gamepads() {
    return detail::input_backend::gamepads();
}

/// Returns the total count of detected input devices.
///
/// @return Number of detected input devices, or the same platform, permission,
/// encoding, malformed-data, and I/O errors as devices().
/// @note The count can change when devices are plugged, unplugged, or reconfigured.
inline result<std::size_t> device_count() {
    return detail::input_backend::device_count();
}

} // namespace input
} // namespace syscape

#endif // SYSCAPE_INPUT_HPP
