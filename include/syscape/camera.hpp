#ifndef SYSCAPE_CAMERA_HPP
#define SYSCAPE_CAMERA_HPP

/// @file
/// @brief Hosted camera devices, webcam enumeration, and non-invasive
/// capability inspection.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Apple mobile platforms expose no permitted public non-invasive camera
/// inventory source to this C++ interface, so all queries report
/// not_supported.
/// @note This module exposes:
/// - Enumeration of camera / video capture devices (devices(),
/// capture_devices()).
/// - Total camera device count (device_count()).
/// - Platform default camera lookup when the platform exposes one
/// (default_device()).
/// - Hardware connection classification (built-in, USB, PCI, virtual).
/// - Lens facing direction (front, back, external).
/// - Non-invasive capabilities (video capture, output, metadata, streaming,
/// touch).
/// - Hardware vendor, product, and revision identifiers.
/// @note Linux queries kernel sysfs (/sys/class/video4linux) and V4L2 ioctls
/// non-invasively.
/// @note Windows 10 version 1803 or later queries the camera-specific SetupAPI
/// device interface; earlier releases return not_supported.
/// @note macOS queries CoreMediaIO hardware property interfaces.
/// @note Device lists and capabilities can change whenever hardware, drivers,
/// permissions, or platform services change. Queries do not cache results.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/camera.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace camera {

/// Facing direction of a camera lens relative to the device/enclosure.
enum class camera_facing : std::uint8_t {
    /// Facing direction is unknown or unspecified.
    unknown,
    /// Front-facing camera (e.g. user-facing laptop webcam or mobile selfie
    /// camera).
    front,
    /// Back-facing camera (e.g. world-facing rear camera).
    back,
    /// External standalone camera (e.g. desktop USB webcam).
    external
};

/// Hardware attachment or implementation classification.
enum class camera_connection : std::uint8_t {
    /// Connection type is unknown or unspecified.
    unknown,
    /// Built-in / integrated camera when no separate transport is exposed.
    built_in,
    /// Universal Serial Bus (USB) attached camera.
    usb,
    /// PCI / PCIe bus attached capture card or interface.
    pci,
    /// Software / virtual camera device.
    virtual_connection
};

/// Hardware vendor, product, and revision identifiers.
struct camera_device_id {
    /// 16-bit Vendor ID (e.g. USB or PCI vendor ID), if exposed.
    std::optional<std::uint16_t> vendor_id;

    /// 16-bit Product / Model ID, if exposed.
    std::optional<std::uint16_t> product_id;

    /// 16-bit Device revision or release number, if exposed.
    std::optional<std::uint16_t> revision;
};

/// Non-invasive hardware capabilities exposed by the camera device driver.
struct camera_capabilities {
    /// Whether the device supports video capture streaming, or no value when
    /// unknown.
    std::optional<bool> has_video_capture;

    /// Whether the device supports video output streaming, or no value when
    /// unknown.
    std::optional<bool> has_video_output;

    /// Whether the device produces metadata capture streams, or no value when
    /// unknown.
    std::optional<bool> has_metadata_capture;

    /// Whether the device supports streaming I/O, or no value when unknown.
    /// The mechanism is platform-specific.
    std::optional<bool> has_streaming;

    /// Whether the device is an optical touch digitizer, or no value when
    /// unknown.
    std::optional<bool> has_touch_device;
};

/// Information describing a single connected or integrated camera device.
struct camera_device {
    /// Unique platform identifier or device node name (e.g. "video0" or device
    /// UID).
    std::string id;

    /// Human-readable camera model or device name (e.g. "Integrated Camera",
    /// "Logitech Webcam C920").
    std::string name;

    /// Character device node path (e.g. "/dev/video0"), if exposed.
    std::optional<std::string> device_path;

    /// Sysfs class path (e.g. "/sys/class/video4linux/video0"), if exposed.
    std::optional<std::string> sysfs_path;

    /// Kernel driver module name (e.g. "uvcvideo"), if exposed.
    std::optional<std::string> driver;

    /// Card or hardware description reported by driver (e.g. "Integrated
    /// Camera: Integrated C"), if exposed.
    std::optional<std::string> card;

    /// Hardware bus topology location (e.g. "usb-0000:06:00.0-1"), if exposed.
    std::optional<std::string> bus_info;

    /// Lens facing orientation.
    camera_facing facing = camera_facing::unknown;

    /// Connection transport classification.
    camera_connection connection = camera_connection::unknown;

    /// Hardware vendor, product, and revision identifiers if at least one is
    /// available.
    std::optional<camera_device_id> hardware_id;

    /// Indicates whether this camera is integrated/built-in vs external.
    std::optional<bool> is_integrated;

    /// Driver-reported non-invasive capabilities, if queryable.
    std::optional<camera_capabilities> capabilities;
};

} // namespace camera
} // namespace syscape

#include <syscape/detail/camera/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__)
#include <syscape/detail/camera/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/camera/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/camera/macos.hpp>
#else
#include <syscape/detail/camera/generic.hpp>
#endif

namespace syscape {
namespace camera {

/// Enumerates all detected camera and video devices.
///
/// @note The result reflects the devices visible to the calling process at
/// query time. Linux, Windows, and macOS have backends; other Hosted Full
/// platforms return not_supported. Enumeration does not start a capture stream.
/// @return A vector of camera_device entries; not_supported when the camera
/// subsystem is unavailable; permission_denied when access is denied;
/// malformed_data for invalid platform data; invalid_encoding for failed native
/// text conversion; or a native I/O error.
inline result<std::vector<camera_device>> devices() {
    return detail::camera_backend::devices();
}

/// Returns the total count of camera and video devices.
///
/// @note The count is a fresh snapshot and can change immediately after return.
/// @return Device count on success; or an error code describing the failure.
inline result<std::size_t> device_count() {
    return detail::camera_backend::device_count();
}

/// Enumerates all camera devices that support video capture.
///
/// @note A device is included only when the backend positively establishes
/// video capture support. Linux currently implements this query; Windows
/// establishes it from the camera interface class; macOS and unknown platforms
/// return not_supported.
/// @return A filtered vector of capture-capable camera_device entries, or an
/// error when capability enumeration is unavailable or fails.
inline result<std::vector<camera_device>> capture_devices() {
    return detail::camera_backend::capture_devices();
}

/// Returns the platform-designated default camera device.
///
/// @note The value can change with platform configuration. The currently
/// implemented backends return not_supported because none exposes an
/// authoritative default-camera selection through the APIs used here.
/// @return The default camera_device entry; not_found if no camera devices
/// exist; not_supported if the platform does not expose a default-camera
/// selection; or an error code describing the lookup failure. This function
/// never guesses a default from enumeration order, connection type, or device
/// name.
inline result<camera_device> default_device() {
    return detail::camera_backend::default_device();
}

} // namespace camera
} // namespace syscape

#endif // SYSCAPE_CAMERA_HPP
