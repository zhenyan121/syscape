#include <cstdint>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

#include <windows.h>

#include <syscape/display.hpp>
#include <syscape/detail/display/common.hpp>
#include <syscape/detail/display/windows.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_utf8_encoding() {
    namespace backend = syscape::detail::display_backend;

    // Valid wide string
    const std::wstring valid_wide = L"Generic PnP Monitor";
    const auto valid_res = backend::wide_to_utf8(valid_wide);
    expect(valid_res.has_value() && *valid_res == "Generic PnP Monitor",
           "Valid wide string must convert to UTF-8");

    // Invalid surrogate
    const wchar_t invalid_wide[] = { static_cast<wchar_t>(0xD800), 0 };
    const auto invalid_res = backend::wide_to_utf8(invalid_wide);
    expect(!invalid_res && invalid_res.error() == syscape::errc::invalid_encoding,
           "Unpaired surrogate must report invalid_encoding");
}

void test_windows_refresh_rate_sentinels() {
    namespace backend = syscape::detail::display_backend;
    expect(!backend::display_frequency_hz(0U).has_value(),
           "Zero refresh rate must remain unknown");
    expect(!backend::display_frequency_hz(1U).has_value(),
           "One is a hardware-default sentinel, not 1 Hz");
    const auto sixty_hz = backend::display_frequency_hz(60U);
    expect(sixty_hz.has_value() && *sixty_hz == 60.0,
           "A real refresh rate must be preserved");
}

void test_windows_orientation_mapping() {
    namespace backend = syscape::detail::display_backend;
    using syscape::display::display_orientation;
    expect(backend::display_mode_orientation(1920U, 1080U, DMDO_DEFAULT) ==
               display_orientation::landscape,
           "An unrotated wide mode must be landscape");
    expect(backend::display_mode_orientation(1080U, 1920U, DMDO_90) ==
               display_orientation::portrait,
           "A clockwise-rotated tall mode must be portrait");
    expect(backend::display_mode_orientation(1080U, 1920U, DMDO_270) ==
               display_orientation::portrait_flipped,
           "A counter-clockwise-rotated tall mode must be flipped portrait");
    expect(backend::display_mode_orientation(1920U, 1080U, DMDO_180) ==
               display_orientation::landscape_flipped,
           "A 180-degree wide mode must be flipped landscape");
}

void test_windows_rect_conversion() {
    namespace backend = syscape::detail::display_backend;
    const RECT valid = {-1920, 0, 0, 1080};
    const auto rect = backend::display_rect_from_native(valid);
    expect(rect.has_value() && rect->x == -1920 && rect->width == 1920U &&
               rect->height == 1080U,
           "Negative desktop origins must preserve a positive extent");
    const RECT inverted = {100, 0, 0, 100};
    const auto invalid = backend::display_rect_from_native(inverted);
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Inverted native bounds must report malformed_data");
}

void test_windows_live_query() {
    const auto disps = syscape::display::displays();
    expect(disps.has_value(), "displays() query must return a result");
    const auto count = syscape::display::display_count();
    expect(count.has_value() && *count == disps->size(),
           "display_count() must match displays() size");
}

} // namespace

int main() {
    test_windows_utf8_encoding();
    test_windows_refresh_rate_sentinels();
    test_windows_orientation_mapping();
    test_windows_rect_conversion();
    test_windows_live_query();
    return failures == 0 ? 0 : 1;
}
