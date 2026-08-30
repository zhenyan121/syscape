#ifndef SYSCAPE_DETAIL_DISPLAY_MACOS_HPP
#define SYSCAPE_DETAIL_DISPLAY_MACOS_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>

#include <syscape/detail/display/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/display.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace display_backend {

class cf_array_guard {
public:
    explicit cf_array_guard(CFArrayRef value) noexcept : value_(value) {}
    cf_array_guard(const cf_array_guard&) = delete;
    cf_array_guard& operator=(const cf_array_guard&) = delete;
    ~cf_array_guard() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }
    CFArrayRef get() const noexcept { return value_; }

private:
    CFArrayRef value_;
};

class display_mode_guard {
public:
    explicit display_mode_guard(CGDisplayModeRef value) noexcept : value_(value) {}
    display_mode_guard(const display_mode_guard&) = delete;
    display_mode_guard& operator=(const display_mode_guard&) = delete;
    ~display_mode_guard() {
        if (value_ != nullptr) { ::CGDisplayModeRelease(value_); }
    }
    CGDisplayModeRef get() const noexcept { return value_; }

private:
    CGDisplayModeRef value_;
};

inline std::optional<double> display_scale_factor(
    std::size_t pixel_width, std::size_t pixel_height,
    double bounds_width, double bounds_height) noexcept {
    if (pixel_width == 0U || pixel_height == 0U ||
        !std::isfinite(bounds_width) || !std::isfinite(bounds_height) ||
        bounds_width <= 0.0 || bounds_height <= 0.0) {
        return std::nullopt;
    }
    const double horizontal = static_cast<double>(pixel_width) / bounds_width;
    const double vertical = static_cast<double>(pixel_height) / bounds_height;
    if (!std::isfinite(horizontal) || !std::isfinite(vertical) ||
        std::abs(horizontal - vertical) > 0.01) {
        return std::nullopt;
    }
    return (horizontal + vertical) / 2.0;
}

inline result<std::uint32_t> display_dimension_u32(std::size_t value) noexcept {
    if (value > static_cast<std::size_t>(
                    (std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<::syscape::display::display_rect> display_rect_from_cg(
    const CGRect& value) noexcept {
    const double x = static_cast<double>(value.origin.x);
    const double y = static_cast<double>(value.origin.y);
    const double width = static_cast<double>(value.size.width);
    const double height = static_cast<double>(value.size.height);
    if (!std::isfinite(x) || !std::isfinite(y) ||
        !std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0 || height <= 0.0) {
        return fail(errc::malformed_data);
    }
    if (x < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
        x > static_cast<double>((std::numeric_limits<std::int32_t>::max)()) ||
        y < static_cast<double>((std::numeric_limits<std::int32_t>::min)()) ||
        y > static_cast<double>((std::numeric_limits<std::int32_t>::max)()) ||
        width > static_cast<double>((std::numeric_limits<std::uint32_t>::max)()) ||
        height > static_cast<double>((std::numeric_limits<std::uint32_t>::max)())) {
        return fail(errc::value_too_large);
    }

    ::syscape::display::display_rect rect;
    rect.x = static_cast<std::int32_t>(x);
    rect.y = static_cast<std::int32_t>(y);
    rect.width = static_cast<std::uint32_t>(width);
    rect.height = static_cast<std::uint32_t>(height);
    return rect;
}

inline result<std::vector<CGDirectDisplayID>> active_display_ids() {
    // Retry if the display set grows between the count and collection calls.
    for (unsigned attempt = 0U; attempt < 3U; ++attempt) {
        uint32_t required_count = 0U;
        CGError err = ::CGGetActiveDisplayList(0U, nullptr, &required_count);
        if (err != kCGErrorSuccess) {
            return fail(errc::io_error);
        }

        std::vector<CGDirectDisplayID> ids(required_count);
        uint32_t actual_count = 0U;
        if (required_count > 0U) {
            err = ::CGGetActiveDisplayList(required_count, ids.data(), &actual_count);
            if (err != kCGErrorSuccess) {
                return fail(errc::io_error);
            }
        }

        uint32_t current_count = 0U;
        err = ::CGGetActiveDisplayList(0U, nullptr, &current_count);
        if (err != kCGErrorSuccess) {
            return fail(errc::io_error);
        }
        if (current_count <= required_count) {
            ids.resize(actual_count);
            return ids;
        }
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::vector<::syscape::display::display_info>> collect_displays() {
    std::vector<::syscape::display::display_info> list;

    const auto display_ids = active_display_ids();
    if (!display_ids) { return fail(display_ids.error()); }
    list.reserve(display_ids->size());

    for (const CGDirectDisplayID did : *display_ids) {
        ::syscape::display::display_info info;
        info.id = std::to_string(did);
        info.state = ::syscape::display::connection_state::connected;
        info.is_primary = (::CGDisplayIsMain(did) != 0);
        info.is_internal = (::CGDisplayIsBuiltin(did) != 0);

        const CGRect bounds = ::CGDisplayBounds(did);
        const auto bounds_rect = display_rect_from_cg(bounds);
        if (!bounds_rect) { return fail(bounds_rect.error()); }
        info.bounds = *bounds_rect;

        const size_t pixel_w = ::CGDisplayPixelsWide(did);
        const size_t pixel_h = ::CGDisplayPixelsHigh(did);
        if (pixel_w > 0U && pixel_h > 0U) {
            const auto width = display_dimension_u32(pixel_w);
            if (!width) { return fail(width.error()); }
            const auto height = display_dimension_u32(pixel_h);
            if (!height) { return fail(height.error()); }
            info.current_width = *width;
            info.current_height = *height;
            info.scale_factor = display_scale_factor(
                pixel_w, pixel_h, bounds.size.width, bounds.size.height);
        }

        const double rot = ::CGDisplayRotation(did);
        if (std::abs(rot - 0.0) < 1.0) {
            info.orientation = ::syscape::display::display_orientation::landscape;
        } else if (std::abs(rot - 90.0) < 1.0) {
            info.orientation = ::syscape::display::display_orientation::portrait;
        } else if (std::abs(rot - 180.0) < 1.0) {
            info.orientation = ::syscape::display::display_orientation::landscape_flipped;
        } else if (std::abs(rot - 270.0) < 1.0) {
            info.orientation = ::syscape::display::display_orientation::portrait_flipped;
        } else {
            info.orientation = ::syscape::display::display_orientation::unknown;
        }

        // Current display mode refresh rate
        const display_mode_guard current_mode(::CGDisplayCopyDisplayMode(did));
        if (current_mode.get() != nullptr) {
            const double hz = ::CGDisplayModeGetRefreshRate(current_mode.get());
            if (hz > 0.0) {
                info.refresh_rate_hz = hz;
            }
        }

        // Enumerate supported display modes
        const cf_array_guard modes_array(::CGDisplayCopyAllDisplayModes(did, nullptr));
        if (modes_array.get() != nullptr) {
            const CFIndex count = ::CFArrayGetCount(modes_array.get());
            for (CFIndex m = 0; m < count; ++m) {
                const auto mode = static_cast<CGDisplayModeRef>(
                    const_cast<void*>(::CFArrayGetValueAtIndex(modes_array.get(), m)));
                if (mode == nullptr) { continue; }
                const size_t mw = ::CGDisplayModeGetPixelWidth(mode);
                const size_t mh = ::CGDisplayModeGetPixelHeight(mode);
                const double mhz = ::CGDisplayModeGetRefreshRate(mode);
                if (mw > 0U && mh > 0U) {
                    const auto mode_width = display_dimension_u32(mw);
                    if (!mode_width) {
                        return fail(mode_width.error());
                    }
                    const auto mode_height = display_dimension_u32(mh);
                    if (!mode_height) {
                        return fail(mode_height.error());
                    }
                    ::syscape::display::display_mode dmode;
                    dmode.width = *mode_width;
                    dmode.height = *mode_height;
                    if (mhz > 0.0) {
                        dmode.refresh_rate_hz = mhz;
                    }
                    bool duplicate = false;
                    for (const auto& existing : info.supported_modes) {
                        if (existing.width == dmode.width && existing.height == dmode.height &&
                            existing.refresh_rate_hz == dmode.refresh_rate_hz) {
                            duplicate = true;
                            break;
                        }
                    }
                    if (!duplicate) {
                        info.supported_modes.push_back(dmode);
                    }
                }
            }
        }

        list.push_back(std::move(info));
    }

    return list;
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
