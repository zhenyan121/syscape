#ifndef SYSCAPE_BLUETOOTH_HPP
#define SYSCAPE_BLUETOOTH_HPP

/// @file
/// @brief Hosted Bluetooth adapters, radio state, capabilities, and device
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Apple mobile platforms expose no permitted public in-process
/// Bluetooth inventory source to this C++ interface, so all queries report
/// not_supported.
/// @note This module exposes:
/// - Enumeration of local Bluetooth host adapters (adapters()).
/// - Total Bluetooth adapter count (adapter_count()).
/// - Platform default Bluetooth adapter lookup (default_adapter()).
/// - Enumeration of paired / bonded remote devices (paired_devices()).
/// - Enumeration of currently connected remote devices (connected_devices()).
/// - Adapter radio power and block states (on, off, blocked).
/// - Adapter bus transport classification (built-in, USB, PCI, UART, SDIO).
/// - Bluetooth SIG company identifiers, HCI and LMP specification versions.
/// - Remote device Bluetooth Class of Device (CoD) decoding and signal RSSI.
/// @note Linux queries sysfs (/sys/class/bluetooth, /sys/class/rfkill) and
/// non-blocking AF_BLUETOOTH HCI socket ioctls.
/// @note Windows queries official Win32 Bluetooth APIs (BluetoothAPIs.h).
/// @note macOS queries Darwin IOKit IORegistry properties.
/// @note Device lists and radio states can change at any time. Queries do not
/// cache results.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/bluetooth.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace bluetooth {

/// Radio power and operational block status of a Bluetooth controller.
enum class adapter_power_state : std::uint8_t {
    /// Power state is unknown or could not be determined.
    unknown,
    /// Adapter is powered on and radio is active.
    on,
    /// Adapter is powered off or disabled in software.
    off,
    /// Adapter is blocked by a software or hardware rfkill switch.
    blocked
};

/// Hardware attachment bus or transport classification.
enum class adapter_bus_type : std::uint8_t {
    /// Transport bus is unknown or unspecified.
    unknown,
    /// Integrated or built-in system controller without distinct external bus.
    built_in,
    /// Universal Serial Bus (USB) attached controller.
    usb,
    /// PCI / PCIe bus attached controller.
    pci,
    /// UART / serial interface attached controller.
    uart,
    /// SDIO attached controller.
    sdio,
    /// Virtual or software Bluetooth emulator.
    virtual_bus
};

/// Major device class decoded from 24-bit Bluetooth Class of Device (CoD).
enum class major_device_class : std::uint8_t {
    /// Device class is unknown or unassigned.
    unknown,
    /// Computer (desktop, laptop, server, PDA).
    computer,
    /// Phone (cellular smartphone, cordless phone, modem).
    phone,
    /// Audio / Video device (headset, headphone, speaker, display, mic).
    audio_video,
    /// Peripheral (keyboard, mouse, touchpad, gamepad, joystick).
    peripheral,
    /// Imaging device (printer, scanner, camera, display).
    imaging,
    /// Wearable device (wrist watch, fitness band, pager).
    wearable,
    /// Toy device (robot, vehicle, doll).
    toy,
    /// Health / medical device (blood pressure, thermometer, scale).
    health,
    /// Network Access Point / LAN bridge.
    network_access_point,
    /// Miscellaneous / other device class.
    miscellaneous
};

/// Information describing a local Bluetooth host controller / adapter.
struct adapter_info {
    /// Unique platform identifier or interface name (e.g. "hci0" on Linux).
    std::string id;

    /// Human-readable adapter name or label (e.g. "Intel Wireless Bluetooth").
    std::string name;

    /// Canonical Bluetooth device MAC address in uppercase colon-separated
    /// format (e.g. "00:1A:7D:DA:71:13"), if exposed.
    std::optional<std::string> address;

    /// Current radio power and block status.
    adapter_power_state power_state = adapter_power_state::unknown;

    /// Hardware bus attachment classification.
    adapter_bus_type bus = adapter_bus_type::unknown;

    /// Whether the adapter is currently discoverable by remote devices.
    std::optional<bool> is_discoverable;

    /// Whether the adapter is currently accepting incoming connections.
    std::optional<bool> is_connectable;

    /// Bluetooth Core Specification HCI version (e.g. 10 for Bluetooth 5.1).
    std::optional<std::uint8_t> hci_version;

    /// HCI revision / manufacturer-defined firmware revision.
    std::optional<std::uint16_t> hci_revision;

    /// Bluetooth Link Manager Protocol (LMP) version.
    std::optional<std::uint8_t> lmp_version;

    /// LMP subversion.
    std::optional<std::uint16_t> lmp_subversion;

    /// 16-bit Company Identifier assigned by the Bluetooth SIG (e.g. 0x0002 for
    /// Intel, 0x000F for Broadcom).
    std::optional<std::uint16_t> manufacturer_id;
};

/// Information describing a paired, remembered, or connected remote Bluetooth
/// device.
struct device_info {
    /// Human-readable device name or advertised friendly name, if known.
    std::optional<std::string> name;

    /// Canonical Bluetooth MAC address (e.g. "AA:BB:CC:DD:EE:FF").
    std::string address;

    /// Platform identifier of the local adapter associated with this device, if
    /// known.
    std::optional<std::string> adapter_id;

    /// Major device classification decoded from Bluetooth Class of Device.
    major_device_class device_type = major_device_class::unknown;

    /// Whether the device is currently connected.
    std::optional<bool> is_connected;

    /// Whether the device is paired / bonded with the host.
    std::optional<bool> is_paired;

    /// Whether the device is marked trusted in the platform store.
    std::optional<bool> is_trusted;

    /// Whether the device is blocked / blacklisted.
    std::optional<bool> is_blocked;

    /// Raw 24-bit Bluetooth Class of Device (CoD), if exposed.
    std::optional<std::uint32_t> class_of_device;

    /// Received Signal Strength Indicator in dBm, if available.
    std::optional<std::int16_t> rssi_dbm;

    /// Battery percentage level (0-100), if exposed.
    std::optional<std::uint8_t> battery_percentage;
};

} // namespace bluetooth
} // namespace syscape

#include <syscape/detail/bluetooth/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__)
#include <syscape/detail/bluetooth/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/bluetooth/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/bluetooth/macos.hpp>
#else
#include <syscape/detail/bluetooth/generic.hpp>
#endif

namespace syscape {
namespace bluetooth {

/// Enumerates all local Bluetooth host controllers / adapters.
///
/// @note The result reflects adapters present and visible at query time. Linux,
/// Windows, and macOS have backends; other platforms return not_supported.
/// @return A vector of adapter_info entries; not_supported when Bluetooth is
/// unavailable; permission_denied when access is denied; malformed_data for
/// invalid platform data; or a native I/O error.
inline result<std::vector<adapter_info>> adapters() {
    return detail::bluetooth_backend::adapters();
}

/// Returns the total count of local Bluetooth host adapters.
///
/// @note The count is a fresh snapshot and can change immediately after return.
/// @return Adapter count on success; or an error code describing the failure.
inline result<std::size_t> adapter_count() {
    return detail::bluetooth_backend::adapter_count();
}

/// Returns the primary or default local Bluetooth host adapter.
///
/// @note When multiple adapters exist, the backend returns the designated
/// default or primary controller (e.g. hci0 on Linux, primary radio on Windows).
/// @return The default adapter_info entry; not_found if no adapters exist;
/// not_supported if the platform does not expose a default adapter concept; or
/// an error code describing the lookup failure.
inline result<adapter_info> default_adapter() {
    return detail::bluetooth_backend::default_adapter();
}

/// Enumerates paired / bonded remote Bluetooth devices known to the system.
///
/// @note Queries local system pairing records non-invasively without triggering
/// radio discovery.
/// @return A vector of device_info entries for paired devices;
/// permission_denied if pairing records cannot be read; not_supported if the
/// platform does not expose pairing records; or an error code.
inline result<std::vector<device_info>> paired_devices() {
    return detail::bluetooth_backend::paired_devices();
}

/// Enumerates remote Bluetooth devices currently connected to any local adapter.
///
/// @note Queries active connection lists from the kernel or controller driver.
/// @return A vector of device_info entries for connected devices;
/// not_supported if connection enumeration is unavailable; or an error code.
inline result<std::vector<device_info>> connected_devices() {
    return detail::bluetooth_backend::connected_devices();
}

} // namespace bluetooth
} // namespace syscape

#endif // SYSCAPE_BLUETOOTH_HPP
