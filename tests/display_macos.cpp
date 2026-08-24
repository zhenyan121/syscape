#include <cmath>
#include <iostream>
#include <string_view>

#include <syscape/display.hpp>
#include <syscape/detail/display/common.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_utf8_validation() {
    const std::string valid = "Color LCD";
    expect(syscape::detail::is_valid_utf8(valid),
           "Valid monitor name must pass UTF-8 validation");

    const std::string invalid = "\xFF\xFE\xFD";
    expect(!syscape::detail::is_valid_utf8(invalid),
           "Invalid UTF-8 byte sequence must fail validation");
}

void test_macos_scale_factor() {
    namespace backend = syscape::detail::display_backend;
    const auto retina = backend::display_scale_factor(2880U, 1800U, 1440.0, 900.0);
    expect(retina.has_value() && std::abs(*retina - 2.0) < 0.001,
           "Matching pixel and desktop dimensions must produce scale 2");
    expect(!backend::display_scale_factor(2880U, 1800U, 0.0, 900.0).has_value(),
           "Zero-sized bounds must not produce a scale");
    expect(!backend::display_scale_factor(2880U, 1800U, 1440.0, 1000.0).has_value(),
           "Inconsistent horizontal and vertical scales must remain unknown");
}

void test_macos_checked_conversions() {
    namespace backend = syscape::detail::display_backend;
    const auto dimension = backend::display_dimension_u32(3840U);
    expect(dimension.has_value() && *dimension == 3840U,
           "A normal pixel dimension must convert exactly");
    const CGRect valid = {{-1920.0, 0.0}, {1920.0, 1080.0}};
    const auto rect = backend::display_rect_from_cg(valid);
    expect(rect.has_value() && rect->x == -1920 && rect->width == 1920U,
           "CoreGraphics desktop bounds must preserve negative origins");
    const CGRect invalid = {{0.0, 0.0}, {0.0, 1080.0}};
    const auto invalid_rect = backend::display_rect_from_cg(invalid);
    expect(!invalid_rect && invalid_rect.error() == syscape::errc::malformed_data,
           "Invalid CoreGraphics bounds must report malformed_data");
}

void test_macos_live_query() {
    const auto disps = syscape::display::displays();
    expect(disps.has_value(), "displays() must return a result");
    const auto count = syscape::display::display_count();
    expect(count.has_value() && *count == disps->size(),
           "display_count() must match displays() size");
}

} // namespace

int main() {
    test_macos_utf8_validation();
    test_macos_scale_factor();
    test_macos_checked_conversions();
    test_macos_live_query();
    return failures == 0 ? 0 : 1;
}
