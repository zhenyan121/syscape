#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <mbctype.h>
#include <syscape/detail/locale/windows.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/locale.hpp>

namespace {

bool valid_label(const syscape::result<std::string>& value) {
    return value && !value->empty() && syscape::detail::is_valid_utf8(*value);
}

/// Renders one synthetic NUL-delimited buffer for the splitter tests.
std::wstring joined(std::initializer_list<std::wstring> parts) {
    std::wstring buffer;
    for (const std::wstring& part : parts) {
        buffer.append(part);
        buffer.push_back(L'\0');
    }
    return buffer;
}

void test_language_name_splitting() {
    namespace backend = syscape::detail::locale_backend;

    const std::wstring two = joined({L"de-de", L"en-US"});
    const auto pair = backend::split_language_names(two.data(), two.size());
    if (!pair || pair->size() != 2U || (*pair)[0] != "de-de" ||
        (*pair)[1] != "en-US") {
        std::exit(20);
    }

    const std::wstring single = joined({L"zh-CN"});
    const auto one = backend::split_language_names(single.data(), single.size());
    if (!one || one->size() != 1U || (*one)[0] != "zh-CN") {
        std::exit(21);
    }

    // Padding after the documented double-null terminator is ignored.
    std::wstring padded = joined({L"en"});
    padded.push_back(L'\0');
    padded.append(L"padding");
    const auto trimmed =
        backend::split_language_names(padded.data(), padded.size());
    if (!trimmed || trimmed->size() != 1U || (*trimmed)[0] != "en") {
        std::exit(22);
    }

    const auto empty = backend::split_language_names(L"", 0U);
    if (!empty || !empty->empty()) {
        std::exit(23);
    }

    // A lone surrogate cannot be converted and must fail the query.
    const std::wstring lone = joined({L"a", L"\xD800"});
    const auto invalid =
        backend::split_language_names(lone.data(), lone.size());
    if (invalid ||
        invalid.error() != syscape::errc::invalid_encoding) {
        std::exit(24);
    }
}

void test_time_zone_key_extraction() {
    namespace backend = syscape::detail::locale_backend;

    const auto named = backend::time_zone_key_name(L"China Standard Time", 20U);
    if (!named || *named != "China Standard Time") {
        std::exit(30);
    }

    // A field terminated exactly within capacity is valid data.
    const auto exact = backend::time_zone_key_name(L"UTC", 4U);
    if (!exact || *exact != "UTC") {
        std::exit(31);
    }

    // An unterminated field is malformed platform data, not truncated text.
    if (backend::time_zone_key_name(L"China Standard Time", 18U) ||
        backend::time_zone_key_name(L"China Standard Time", 18U).error() !=
            syscape::errc::malformed_data) {
        std::exit(32);
    }

    // An empty recording carries no usable name.
    if (backend::time_zone_key_name(L"", 8U) ||
        backend::time_zone_key_name(L"", 8U).error() !=
            syscape::errc::malformed_data) {
        std::exit(33);
    }

    // An unconvertible recording fails conversion instead of corrupting.
    if (backend::time_zone_key_name(L"\xDC00x", 16U) ||
        backend::time_zone_key_name(L"\xDC00x", 16U).error() !=
            syscape::errc::invalid_encoding) {
        std::exit(34);
    }
}

} // namespace

int main() {
    if (syscape::detail::locale_backend::code_page_label(1252) != "1252" ||
        syscape::detail::locale_backend::code_page_label(65001) != "65001") {
        return 1;
    }

    test_language_name_splitting();
    test_time_zone_key_extraction();

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

    const char* const reference_locale_ptr = std::setlocale(LC_ALL, nullptr);
    if (reference_locale_ptr == nullptr) {
        return 9;
    }
    const std::string reference_locale(reference_locale_ptr);
    const auto locale = syscape::locale::current_locale();
    if (!locale || *locale != reference_locale) {
        return 9;
    }

    const std::string reference_encoding = std::to_string(::_getmbcp());
    const auto encoding = syscape::locale::text_encoding();
    if (!encoding || *encoding != reference_encoding) { return 10; }

    if (!valid_label(locale)) { return 11; }
    if (!valid_label(encoding)) { return 12; }

    const auto offset = syscape::locale::utc_offset_seconds();
    if (!offset || *offset <= -86400 || *offset >= 86400) { return 13; }

    const auto languages = syscape::locale::preferred_languages();
    if (!languages || languages->empty()) { return 14; }
    for (const std::string& language : *languages) {
        if (language.empty() || !syscape::detail::is_valid_utf8(language)) {
            return 15;
        }
    }

    const auto region = syscape::locale::country_region_code();
    if (!valid_label(region)) { return 16; }

    const auto zone = syscape::locale::time_zone_identifier();
    if (!valid_label(zone)) { return 17; }

    return 0;
}
