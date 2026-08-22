#include <clocale>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>

#include <mbctype.h>
#include <syscape/detail/locale/windows.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/locale.hpp>

namespace {

bool valid_label(const syscape::result<std::string>& value) {
    return value && !value->empty() && syscape::detail::is_valid_utf8(*value);
}

} // namespace

int main() {
    if (syscape::detail::locale_backend::code_page_label(1252) != "1252" ||
        syscape::detail::locale_backend::code_page_label(65001) != "65001") {
        return 1;
    }

    const auto utc =
        syscape::detail::locale_backend::active_offset_seconds(0, 0);
    if (!utc || *utc != 0) { return 2; }

    const auto china_standard =
        syscape::detail::locale_backend::active_offset_seconds(-480, 0);
    if (!china_standard || *china_standard != 28800) { return 3; }

    const auto eastern_daylight =
        syscape::detail::locale_backend::active_offset_seconds(300, -60);
    if (!eastern_daylight || *eastern_daylight != -14400) { return 4; }

    const auto india_standard =
        syscape::detail::locale_backend::active_offset_seconds(-330, 0);
    if (!india_standard || *india_standard != 19800) { return 5; }

    const auto oversized =
        syscape::detail::locale_backend::active_offset_seconds(20000, 0);
    if (oversized ||
        oversized.error() != syscape::errc::malformed_data) {
        return 6;
    }

    const auto positive_overflow =
        syscape::detail::locale_backend::active_offset_seconds(
            (std::numeric_limits<::LONG>::max)(),
            (std::numeric_limits<::LONG>::max)());
    if (positive_overflow ||
        positive_overflow.error() != syscape::errc::malformed_data) {
        return 7;
    }

    const auto negative_overflow =
        syscape::detail::locale_backend::active_offset_seconds(
            (std::numeric_limits<::LONG>::min)(),
            (std::numeric_limits<::LONG>::min)());
    if (negative_overflow ||
        negative_overflow.error() != syscape::errc::malformed_data) {
        return 8;
    }

    const char* const reference_locale = std::setlocale(LC_ALL, nullptr);
    const auto locale = syscape::locale::current_locale();
    if (!locale || reference_locale == nullptr || *locale != reference_locale) {
        return 9;
    }

    const std::string reference_encoding = std::to_string(::_getmbcp());
    const auto encoding = syscape::locale::text_encoding();
    if (!encoding || *encoding != reference_encoding) { return 10; }

    if (!valid_label(locale)) { return 11; }
    if (!valid_label(encoding)) { return 12; }

    const auto offset = syscape::locale::utc_offset_seconds();
    if (!offset || *offset <= -86400 || *offset >= 86400) { return 13; }

    return 0;
}
