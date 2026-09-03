#ifndef SYSCAPE_DISPLAY_HPP
#define SYSCAPE_DISPLAY_HPP

/// @file
/// @brief Hosted display, monitor, screen geometry, and resolution queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Apple mobile platforms expose no permitted public display inventory
/// source to this C++ interface, so all queries report not_supported.
/// @note This module exposes:
/// - Enumeration of display devices and monitors (displays()) and monitor
/// counts (display_count()).
/// - Identification of the primary display (primary_display()).
/// - Desktop coordinate bounds, work area, and resolution dimensions.
/// - Operating refresh rate in Hz, color depth in bits per pixel, and display
/// orientation.
/// - Physical screen dimensions in millimeters from EDID or OS properties.
/// - Internal/built-in screen classification (e.g. laptop panels) and
/// connection state.
/// - Enumeration of supported display resolution and refresh modes.
/// @note Linux queries sysfs DRM interfaces (/sys/class/drm) and parses VESA
/// EDID binary blocks. Connector sysfs does not expose compositor-owned current
/// mode, desktop layout, scale, orientation, work area, or primary-display
/// selection, so those Linux fields remain absent.
/// @note Windows queries Win32 display monitor and device mode interfaces.
/// @note macOS queries CoreGraphics display management interfaces.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/display.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace display {

/// Display orientation or rotation relative to default upright landscape layout.
enum class display_orientation : std::uint8_t {
    /// The orientation is unknown or cannot be determined.
    unknown,
    /// Standard landscape orientation (0 degrees).
    landscape,
    /// Portrait orientation (90 degrees clockwise).
    portrait,
    /// Inverted landscape orientation (180 degrees).
    landscape_flipped,
    /// Inverted portrait orientation (270 degrees clockwise).
    portrait_flipped
};

/// Connection state of a display connector or video output.
enum class connection_state : std::uint8_t {
    /// Connection state is unknown or undetermined.
    unknown,
    /// Display device is connected and detected.
    connected,
    /// Display connector is disconnected or inactive.
    disconnected
};

/// 2D rectangle describing coordinates and dimensions on the virtual desktop.
struct display_rect {
    /// Horizontal origin coordinate in pixels (can be negative in multi-monitor setups).
    std::int32_t x = 0;
    /// Vertical origin coordinate in pixels (can be negative in multi-monitor setups).
    std::int32_t y = 0;
    /// Width in pixels.
    std::uint32_t width = 0U;
    /// Height in pixels.
    std::uint32_t height = 0U;
};

/// A supported display resolution and optional refresh rate mode.
struct display_mode {
    /// Horizontal resolution in pixels.
    std::uint32_t width = 0U;
    /// Vertical resolution in pixels.
    std::uint32_t height = 0U;
    /// Nominal refresh rate in Hz, if known.
    std::optional<double> refresh_rate_hz;
};

/// Information describing a single display, monitor, or video output.
struct display_info {
    /// Platform-specific display or connector identifier (e.g. "card1-eDP-1", "\\\\.\\DISPLAY1", or display ID).
    std::string id;

    /// User-friendly monitor name, model description, or product label as UTF-8, if exposed.
    std::optional<std::string> name;

    /// Three-letter PNP manufacturer ID (e.g. "DEL", "SAM", "TMA") or manufacturer name, if exposed.
    std::optional<std::string> manufacturer;

    /// Output connector type (e.g. "eDP", "HDMI-A", "DP", "VGA", "DVI-D", "Virtual"), if exposed.
    std::optional<std::string> connector_type;

    /// Indicates whether this display is the primary desktop or boot display.
    bool is_primary = false;

    /// Indicates whether this display is an internal / built-in panel (e.g. laptop eDP/LVDS screen).
    bool is_internal = false;

    /// Physical or logical connection state of the display.
    connection_state state = connection_state::unknown;

    /// Desktop coordinate bounds (origin and size) in virtual desktop pixels, if active.
    std::optional<display_rect> bounds;

    /// Usable work area bounds (excluding taskbars, docks, and system panels), if exposed.
    std::optional<display_rect> work_area;

    /// Current active horizontal resolution in pixels, if known.
    std::optional<std::uint32_t> current_width;

    /// Current active vertical resolution in pixels, if known.
    std::optional<std::uint32_t> current_height;

    /// Current operating refresh rate in Hz, if known.
    std::optional<double> refresh_rate_hz;

    /// Color depth in bits per pixel (e.g. 24, 30, 32), if known.
    std::optional<std::uint32_t> bits_per_pixel;

    /// DPI scale factor (e.g. 1.0, 1.25, 1.5, 2.0), if exposed by the platform.
    std::optional<double> scale_factor;

    /// Screen orientation / rotation, if known.
    std::optional<display_orientation> orientation;

    /// Physical screen width in millimeters, if exposed by EDID or the platform.
    std::optional<std::uint32_t> physical_width_mm;

    /// Physical screen height in millimeters, if exposed by EDID or the platform.
    std::optional<std::uint32_t> physical_height_mm;

    /// List of supported display resolution modes exposed by the platform or EDID.
    std::vector<display_mode> supported_modes;
};

} // namespace display
} // namespace syscape

#include <syscape/detail/display/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/display/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/display/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/display/macos.hpp>
#else
#include <syscape/detail/display/generic.hpp>
#endif

namespace syscape {
namespace display {

/// Enumerates all detected displays, monitors, and video outputs.
///
/// @return A vector of display_info structures, an empty vector if no displays are detected,
/// or an error.
inline result<std::vector<display_info>> displays() {
    return detail::display_backend::displays();
}

/// Returns the number of detected displays and monitors.
///
/// @return The count of displays, or an error.
inline result<std::size_t> display_count() {
    return detail::display_backend::display_count();
}

/// Returns the primary display or main desktop monitor.
///
/// @return The primary display_info, not_found if no primary display is identified,
/// or an error.
inline result<display_info> primary_display() {
    return detail::display_backend::primary_display();
}

} // namespace display
} // namespace syscape

#endif
