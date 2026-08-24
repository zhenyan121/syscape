#ifndef SYSCAPE_DETAIL_DISPLAY_WINDOWS_HPP
#define SYSCAPE_DETAIL_DISPLAY_WINDOWS_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <windows.h>

#include <syscape/detail/display/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/display.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace display_backend {

inline result<std::string> wide_to_utf8(std::wstring_view value) {
    static_assert(sizeof(wchar_t) == sizeof(char16_t),
                  "The Windows backend requires 16-bit wchar_t");
    std::u16string converted;
    converted.reserve(value.size());
    for (wchar_t unit : value) {
        converted.push_back(static_cast<char16_t>(unit));
    }
    return utf16_to_utf8(converted);
}

inline std::optional<double> display_frequency_hz(DWORD value) noexcept {
    if (value <= 1U) {
        return std::nullopt;
    }
    return static_cast<double>(value);
}

inline std::optional<::syscape::display::display_orientation> display_mode_orientation(
    DWORD width, DWORD height, DWORD rotation) noexcept {
    if (width == 0U || height == 0U) {
        return std::nullopt;
    }
    const bool landscape = width >= height;
    switch (rotation) {
    case DMDO_DEFAULT:
        return landscape
                   ? ::syscape::display::display_orientation::landscape
                   : ::syscape::display::display_orientation::portrait;
    case DMDO_180:
        return landscape
                   ? ::syscape::display::display_orientation::landscape_flipped
                   : ::syscape::display::display_orientation::portrait_flipped;
    case DMDO_90:
        if (!landscape) {
            return ::syscape::display::display_orientation::portrait;
        }
        break;
    case DMDO_270:
        if (!landscape) {
            return ::syscape::display::display_orientation::portrait_flipped;
        }
        break;
    default:
        break;
    }
    return ::syscape::display::display_orientation::unknown;
}

inline std::error_code last_system_error_or_io_error() noexcept {
    const DWORD value = ::GetLastError();
    if (value == ERROR_SUCCESS) {
        return make_error_code(errc::io_error);
    }
    return std::error_code(static_cast<int>(value), std::system_category());
}

inline result<::syscape::display::display_rect> display_rect_from_native(
    const RECT& value) noexcept {
    const std::int64_t width = static_cast<std::int64_t>(value.right) -
                               static_cast<std::int64_t>(value.left);
    const std::int64_t height = static_cast<std::int64_t>(value.bottom) -
                                static_cast<std::int64_t>(value.top);
    if (width <= 0 || height <= 0) {
        return fail(errc::malformed_data);
    }
    if (width > static_cast<std::int64_t>(
                    (std::numeric_limits<std::uint32_t>::max)()) ||
        height > static_cast<std::int64_t>(
                     (std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }

    ::syscape::display::display_rect rect;
    rect.x = static_cast<std::int32_t>(value.left);
    rect.y = static_cast<std::int32_t>(value.top);
    rect.width = static_cast<std::uint32_t>(width);
    rect.height = static_cast<std::uint32_t>(height);
    return rect;
}

struct monitor_collector_context {
    std::vector<::syscape::display::display_info> displays;
    std::error_code error;
};

inline BOOL CALLBACK monitor_enum_proc(
    HMONITOR h_monitor, HDC /*hdc*/, LPRECT /*lprc_monitor*/, LPARAM l_param) {
    auto* ctx = reinterpret_cast<monitor_collector_context*>(l_param);

    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    if (!::GetMonitorInfoW(h_monitor, reinterpret_cast<LPMONITORINFO>(&mi))) {
        ctx->error = last_system_error_or_io_error();
        return FALSE;
    }

    const auto id_utf8 = wide_to_utf8(mi.szDevice);
    if (!id_utf8) {
        ctx->error = id_utf8.error();
        return FALSE;
    }

    ::syscape::display::display_info info;
    info.id = *id_utf8;
    info.state = ::syscape::display::connection_state::connected;
    info.is_primary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

    // Bounds rectangle
    const auto bounds_rect = display_rect_from_native(mi.rcMonitor);
    if (!bounds_rect) {
        ctx->error = bounds_rect.error();
        return FALSE;
    }
    info.bounds = *bounds_rect;

    // Work area rectangle
    const auto work_rect = display_rect_from_native(mi.rcWork);
    if (!work_rect) {
        ctx->error = work_rect.error();
        return FALSE;
    }
    info.work_area = *work_rect;

    // Query current display settings
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    if (::EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
        info.current_width = dm.dmPelsWidth;
        info.current_height = dm.dmPelsHeight;
        if (dm.dmBitsPerPel > 0U) {
            info.bits_per_pixel = dm.dmBitsPerPel;
        }
        info.refresh_rate_hz = display_frequency_hz(dm.dmDisplayFrequency);
        info.orientation = display_mode_orientation(
            dm.dmPelsWidth, dm.dmPelsHeight, dm.dmDisplayOrientation);
    }

    // Enumerate supported display modes
    DWORD mode_index = 0U;
    DEVMODEW mode_dm{};
    mode_dm.dmSize = sizeof(mode_dm);
    while (::EnumDisplaySettingsW(mi.szDevice, mode_index, &mode_dm)) {
        ++mode_index;
        ::syscape::display::display_mode m;
        m.width = mode_dm.dmPelsWidth;
        m.height = mode_dm.dmPelsHeight;
        m.refresh_rate_hz = display_frequency_hz(mode_dm.dmDisplayFrequency);
        // Avoid duplicate identical modes
        bool duplicate = false;
        for (const auto& existing : info.supported_modes) {
            if (existing.width == m.width && existing.height == m.height &&
                existing.refresh_rate_hz == m.refresh_rate_hz) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            info.supported_modes.push_back(m);
        }
    }

    // Query the monitor label. DISPLAY_DEVICE::DeviceID is not a documented
    // portable PNP-ID source for this call, so do not infer a manufacturer
    // from its implementation-specific contents.
    DISPLAY_DEVICEW dd{};
    dd.cb = sizeof(dd);
    if (::EnumDisplayDevicesW(mi.szDevice, 0, &dd, 0)) {
        const auto name_utf8 = wide_to_utf8(dd.DeviceString);
        if (!name_utf8) {
            ctx->error = name_utf8.error();
            return FALSE;
        }
        if (!name_utf8->empty()) {
            info.name = *name_utf8;
        }
    }

    ctx->displays.push_back(std::move(info));
    return TRUE;
}

inline result<std::vector<::syscape::display::display_info>> collect_displays() {
    monitor_collector_context ctx;
    if (!::EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(&ctx))) {
        if (ctx.error != std::error_code()) {
            return fail(ctx.error);
        }
        return fail(last_system_error_or_io_error());
    }
    if (ctx.error != std::error_code()) {
        return fail(ctx.error);
    }
    return ctx.displays;
}

inline result<std::vector<::syscape::display::display_info>> displays() {
    return collect_displays();
}

inline result<std::size_t> display_count() {
    const auto res = collect_displays();
    if (!res) { return fail(res.error()); }
    return res->size();
}

inline result<::syscape::display::display_info> primary_display() {
    const auto res = collect_displays();
    if (!res) { return fail(res.error()); }
    if (res->empty()) { return fail(errc::not_found); }

    for (const auto& d : *res) {
        if (d.is_primary) {
            return d;
        }
    }
    return fail(errc::not_found);
}

} // namespace display_backend
} // namespace detail
} // namespace syscape

#endif
