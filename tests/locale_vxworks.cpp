#include <cstdlib>
#include <iostream>
#include <string>

#include <syscape/locale.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class env_var_guard {
    public:
    explicit env_var_guard(const char* name) : name_(name) {
        const char* const val = std::getenv(name);
        if (val != nullptr) {
            original_value_ = val;
            has_original_ = true;
        }
    }

    ~env_var_guard() {
        if (has_original_) {
            ::setenv(name_.c_str(), original_value_.c_str(), 1);
        } else {
            ::unsetenv(name_.c_str());
        }
    }

    env_var_guard(const env_var_guard&) = delete;
    env_var_guard& operator=(const env_var_guard&) = delete;

    private:
    std::string name_;
    std::string original_value_;
    bool has_original_ = false;
};

void test_locale_queries() {
    const env_var_guard tz_guard("TZ");
    const env_var_guard timezone_guard("TIMEZONE");
    ::unsetenv("TIMEZONE");

    const auto loc = syscape::locale::current_locale();
    expect(loc && !loc->empty(), "current locale query must succeed");

    const auto enc = syscape::locale::text_encoding();
    expect(!enc && enc.error() == syscape::errc::not_supported,
           "text encoding query must report not_supported on VxWorks");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(offset.has_value(), "utc offset query must succeed");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(tz.has_value() || tz.error() == syscape::errc::not_found ||
               tz.error() == syscape::errc::not_supported,
           "time zone query must succeed or report a documented error");

    const auto langs = syscape::locale::preferred_languages();
    expect(langs.error() == syscape::errc::not_supported,
           "preferred languages must report not_supported on VxWorks");

    const auto country = syscape::locale::country_region_code();
    expect(country.error() == syscape::errc::not_supported,
           "country region code must report not_supported on VxWorks");
}

void test_timezone_overrides() {
    const env_var_guard tz_guard("TZ");
    const env_var_guard timezone_guard("TIMEZONE");

    ::unsetenv("TIMEZONE");
    ::unsetenv("TZ");

    // POSIX rule string must return not_found
    ::setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1);
    const auto rule_res = syscape::locale::time_zone_identifier();
    expect(!rule_res && rule_res.error() == syscape::errc::not_found,
           "POSIX rule string must report not_found");

    // Colon-prefixed UTC
    ::setenv("TZ", ":UTC", 1);
    const auto utc_colon = syscape::locale::time_zone_identifier();
    expect(utc_colon && *utc_colon == "UTC", ":UTC must resolve to UTC");

    // Path outside /usr/share/zoneinfo/ must fail
    ::setenv("TZ", ":/tmp/custom-zoneinfo/Asia/Shanghai", 1);
    const auto custom_path = syscape::locale::time_zone_identifier();
    expect(!custom_path, "path outside /usr/share/zoneinfo must fail");

    // Invalid UTF-8 in TZ must fail with invalid_encoding
    ::setenv("TZ", "\xFF\xFF", 1);
    const auto bad_tz = syscape::locale::time_zone_identifier();
    expect(!bad_tz && bad_tz.error() == syscape::errc::invalid_encoding,
           "invalid UTF-8 TZ must fail with invalid_encoding");

    ::unsetenv("TZ");

    // TIMEZONE environment parameter support (VxWorks native configuration)
    ::setenv("TIMEZONE", "CET::-60:032502:102803", 1);
    const auto cet_id = syscape::locale::time_zone_identifier();
    expect(cet_id && *cet_id == "CET",
           "TIMEZONE CET::-60 must parse identifier CET");

    ::setenv("TIMEZONE", "EST::300:040102:102602", 1);
    const auto est_id = syscape::locale::time_zone_identifier();
    expect(est_id && *est_id == "EST",
           "TIMEZONE EST::300 must parse identifier EST");

    // Malformed TIMEZONE must fail with malformed_data in both identifier and
    // offset
    ::setenv("TIMEZONE", "malformed:::broken", 1);
    const auto bad_id = syscape::locale::time_zone_identifier();
    expect(!bad_id && bad_id.error() == syscape::errc::malformed_data,
           "malformed TIMEZONE parameter must fail with malformed_data");
    const auto bad_offset = syscape::locale::utc_offset_seconds();
    expect(!bad_offset && bad_offset.error() == syscape::errc::malformed_data,
           "malformed TIMEZONE must cause utc_offset_seconds to fail with "
           "malformed_data");

    // Invalid UTF-8 in TIMEZONE must fail with invalid_encoding in both
    ::setenv("TIMEZONE", "\xFF\xFF::-60", 1);
    const auto bad_utf8_id = syscape::locale::time_zone_identifier();
    expect(!bad_utf8_id &&
               bad_utf8_id.error() == syscape::errc::invalid_encoding,
           "invalid UTF-8 TIMEZONE must fail with invalid_encoding");
    const auto bad_utf8_offset = syscape::locale::utc_offset_seconds();
    expect(!bad_utf8_offset &&
               bad_utf8_offset.error() == syscape::errc::invalid_encoding,
           "invalid UTF-8 TIMEZONE must cause utc_offset_seconds to fail with "
           "invalid_encoding");

    ::unsetenv("TIMEZONE");
}

void test_vxworks_timezone_helpers() {
    auto tz_cet = syscape::detail::locale_backend::parse_vxworks_timezone(
        "CET::-60:032502:102803");
    expect(tz_cet.has_value() && tz_cet->name == "CET" &&
               tz_cet->offset_seconds == 3600 && tz_cet->has_dst,
           "parse_vxworks_timezone must parse CET and 3600 seconds with DST");

    auto tz_est = syscape::detail::locale_backend::parse_vxworks_timezone(
        "EST::300:040102:102602");
    expect(tz_est.has_value() && tz_est->name == "EST" &&
               tz_est->offset_seconds == -18000 && tz_est->has_dst,
           "parse_vxworks_timezone must parse EST and -18000 seconds with DST");

    auto tz_utc =
        syscape::detail::locale_backend::parse_vxworks_timezone("UTC::0");
    expect(tz_utc.has_value() && tz_utc->name == "UTC" &&
               tz_utc->offset_seconds == 0 && !tz_utc->has_dst,
           "parse_vxworks_timezone must parse UTC::0 without DST");

    auto tz_bad = syscape::detail::locale_backend::parse_vxworks_timezone(
        "broken:::format");
    expect(!tz_bad && tz_bad.error() == syscape::errc::malformed_data,
           "malformed timezone string must fail with malformed_data");

    // Only DST start (4 tokens) must be rejected as malformed_data
    auto tz_dst_start_only =
        syscape::detail::locale_backend::parse_vxworks_timezone(
            "CET::-60:032502");
    expect(!tz_dst_start_only &&
               tz_dst_start_only.error() == syscape::errc::malformed_data,
           "timezone with only DST start must fail with malformed_data");

    // Only DST end (5 tokens with empty start) must be rejected
    auto tz_dst_end_only =
        syscape::detail::locale_backend::parse_vxworks_timezone(
            "CET::-60::102803");
    expect(!tz_dst_end_only &&
               tz_dst_end_only.error() == syscape::errc::malformed_data,
           "timezone with only DST end must fail with malformed_data");

    // Out-of-bounds month/day/hour in DST must be rejected
    auto tz_dst_bad_month =
        syscape::detail::locale_backend::parse_vxworks_timezone(
            "CET::-60:999999:000000");
    expect(!tz_dst_bad_month &&
               tz_dst_bad_month.error() == syscape::errc::malformed_data,
           "DST with month 99 must fail with malformed_data");

    auto tz_dst_bad_day =
        syscape::detail::locale_backend::parse_vxworks_timezone(
            "CET::-60:043102:102803");
    expect(!tz_dst_bad_day &&
               tz_dst_bad_day.error() == syscape::errc::malformed_data,
           "DST with day 31 in April must fail with malformed_data");

    auto tz_dst_bad_hour =
        syscape::detail::locale_backend::parse_vxworks_timezone(
            "CET::-60:032524:102803");
    expect(!tz_dst_bad_hour &&
               tz_dst_bad_hour.error() == syscape::errc::malformed_data,
           "DST with hour 24 must fail with malformed_data");

    // Invalid UTF-8 name must return invalid_encoding
    auto tz_bad_utf8 = syscape::detail::locale_backend::parse_vxworks_timezone(
        "\xFF\xFF::-60");
    expect(!tz_bad_utf8 &&
               tz_bad_utf8.error() == syscape::errc::invalid_encoding,
           "timezone with invalid UTF-8 name must fail with invalid_encoding");

    // Test calculate_tm_difference without relying on strftime("%z")
    std::tm local {};
    std::tm gm {};
    local.tm_year = 126; // 2026
    local.tm_yday = 200;
    local.tm_hour = 13;
    local.tm_min = 30;
    local.tm_sec = 0;

    gm.tm_year = 126;
    gm.tm_yday = 200;
    gm.tm_hour = 5;
    gm.tm_min = 30;
    gm.tm_sec = 0;

    auto diff_same_day =
        syscape::detail::locale_backend::calculate_tm_difference(local, gm);
    expect(diff_same_day.has_value() && *diff_same_day == 28800,
           "calculate_tm_difference must compute +28800 (+8h) correctly");

    // Year boundary crossing: local is 2026-01-01 02:00, gm is 2025-12-31 21:00
    // (+5h)
    local.tm_year = 126;
    local.tm_yday = 0;
    local.tm_hour = 2;
    local.tm_min = 0;
    local.tm_sec = 0;

    gm.tm_year = 125;
    gm.tm_yday = 364; // 2025 is not leap, 365 days, yday 364
    gm.tm_hour = 21;
    gm.tm_min = 0;
    gm.tm_sec = 0;

    auto diff_cross_year =
        syscape::detail::locale_backend::calculate_tm_difference(local, gm);
    expect(diff_cross_year.has_value() && *diff_cross_year == 18000,
           "calculate_tm_difference across year boundary must compute +18000 "
           "(+5h)");
}

} // namespace

int main() {
    const env_var_guard tz_guard("TZ");
    const env_var_guard timezone_guard("TIMEZONE");

    test_locale_queries();
    test_timezone_overrides();
    test_vxworks_timezone_helpers();
    return failures == 0 ? 0 : 1;
}
