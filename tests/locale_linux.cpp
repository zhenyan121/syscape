#include <cctype>
#include <cerrno>
#include <clocale>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>
#include <time.h>
#include <vector>

#include <langinfo.h>
#include <unistd.h>

#include <syscape/detail/locale/common.hpp>
#include <syscape/detail/locale/linux.hpp>
#include <syscape/detail/locale/posix.hpp>
#include <syscape/detail/locale/preferences_linux.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/locale.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_offset_parsing() {
    const auto plus_eight =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0800");
    expect(plus_eight && *plus_eight == 28800,
           "A +0800 rendering must parse to 28800 seconds");

    const auto minus_five_thirty =
        syscape::detail::locale_backend::parse_gmt_offset_text("-0530");
    expect(minus_five_thirty && *minus_five_thirty == -19800,
           "A -0530 rendering must parse to -19800 seconds");

    const auto zero =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0000");
    expect(zero && *zero == 0,
           "A +0000 rendering must parse to zero, a valid offset");

    const auto extended =
        syscape::detail::locale_backend::parse_gmt_offset_text("+010101");
    expect(extended && *extended == 3661,
           "An extended +010101 rendering must parse to 3661 seconds");

    const auto negative_extended =
        syscape::detail::locale_backend::parse_gmt_offset_text("-020202");
    expect(negative_extended && *negative_extended == -7322,
           "A negative extended rendering keeps its sign");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("") &&
               syscape::detail::locale_backend::parse_gmt_offset_text("")
                       .error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty rendering is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+800"),
           "A truncated rendering is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("0800"),
           "A rendering without a sign is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+2400"),
           "Hours beyond 23 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+0060"),
           "Minutes beyond 59 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+000060")
               || syscape::detail::locale_backend::parse_gmt_offset_text(
                       "+000060")
                          .error() ==
                      syscape::make_error_code(
                          syscape::errc::malformed_data),
           "Seconds beyond 59 are malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+0a00"),
           "A non-digit character is malformed platform data");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+08000"),
           "An over-long rendering is malformed platform data");
}

void test_text_boundaries() {
    const auto valid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>("zh_CN.UTF-8"));
    expect(valid && *valid == "zh_CN.UTF-8",
           "A valid UTF-8 label passes the public boundary");

    const auto empty =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(""));
    expect(!empty &&
               empty.error() == syscape::make_error_code(
                                    syscape::errc::malformed_data),
           "An empty label is rejected as malformed platform data");

    const auto invalid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(std::string(1U, static_cast<char>(0xff))));
    expect(!invalid &&
               invalid.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 label must fail at the public boundary");

    const auto zero_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(0));
    expect(zero_offset && *zero_offset == 0,
           "Zero is a valid offset, never an error sentinel");

    const auto extreme_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(86399));
    expect(extreme_offset && *extreme_offset == 86399,
           "The largest representable day fraction is a valid offset");

    const auto day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(86400));
    expect(!day_offset &&
               day_offset.error() == syscape::make_error_code(
                                         syscape::errc::malformed_data),
           "A full-day offset is malformed platform data");

    const auto negative_day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(-86400));
    expect(!negative_day_offset &&
               negative_day_offset.error() == syscape::errc::malformed_data,
           "A negative full-day offset is malformed platform data");
}

/// Independently parses a "%z" rendering with the C standard-library scan
/// functions so that the backend parser is cross-checked rather than
/// re-executed.
bool reference_offset(const std::tm& local, long& seconds) {
    char formatted[16];
    const std::size_t length =
        std::strftime(formatted, sizeof(formatted), "%z", &local);
    if (length != 5U && length != 7U) { return false; }
    if (formatted[0] != '+' && formatted[0] != '-') { return false; }
    if (!std::isdigit(static_cast<unsigned char>(formatted[1])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[2])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[3])) ||
        !std::isdigit(static_cast<unsigned char>(formatted[4])) ||
        (length == 7U &&
         (!std::isdigit(static_cast<unsigned char>(formatted[5])) ||
          !std::isdigit(static_cast<unsigned char>(formatted[6]))))) {
        return false;
    }

    int hours = 0;
    int minutes = 0;
    int offset_seconds = 0;
    const int fields = std::sscanf(formatted + 1, "%2d%2d%2d", &hours,
                                   &minutes, &offset_seconds);
    if (fields != (length == 7U ? 3 : 2) || hours > 23 || minutes > 59 ||
        offset_seconds > 59) {
        return false;
    }
    seconds = (hours * 3600L + minutes * 60L + offset_seconds) *
              (formatted[0] == '-' ? -1L : 1L);
    return true;
}

void test_runtime_queries() {
    const char* reference_locale = ::setlocale(LC_ALL, nullptr);
    const auto locale = syscape::locale::current_locale();
    expect(locale && reference_locale != nullptr && *locale == reference_locale,
           "Linux must report the C runtime's locale string verbatim");

    const char* reference_encoding = ::nl_langinfo(CODESET);
    const auto encoding = syscape::locale::text_encoding();
    expect(encoding && reference_encoding != nullptr &&
               *encoding == reference_encoding,
           "Linux must report the LC_CTYPE codeset name verbatim");

    ::tzset();
    const std::time_t now = ::time(nullptr);
    std::tm local {};
    expect(::localtime_r(&now, &local) != nullptr,
           "The localtime_r reference lookup must not fail natively");

    long reference_seconds = 0;
    const auto offset = syscape::locale::utc_offset_seconds();
    if (reference_offset(local, reference_seconds)) {
        expect(offset && *offset == static_cast<std::int32_t>(reference_seconds),
               "Linux must report the current UTC offset from the zone rules");
    } else {
        expect(!offset &&
                   offset.error() ==
                       syscape::make_error_code(syscape::errc::not_found),
               "An indeterminable reference zone must report not_found");
    }
}

void test_zone_identifier_extraction() {
    namespace backend = syscape::detail::locale_backend;
    constexpr std::string_view root = "/usr/share/zoneinfo";

    const auto relative = backend::zone_identifier_from_target(
        std::string_view("../usr/share/zoneinfo/Asia/Shanghai"), root);
    expect(relative && *relative == "Asia/Shanghai",
           "A relative zoneinfo target records the suffix as identifier");

    const auto absolute = backend::zone_identifier_from_target(
        std::string_view("/usr/share/zoneinfo/Etc/UTC"), root);
    expect(absolute && *absolute == "Etc/UTC",
           "An absolute zoneinfo target records the suffix as identifier");

    const auto dotted = backend::zone_identifier_from_target(
        std::string_view("../usr/share/zoneinfo/America/Argentina/../"
                         "Buenos_Aires"),
        root);
    expect(dotted && *dotted == "America/Buenos_Aires",
           "Dot segments resolve lexically before extraction");

    const auto root_itself = backend::zone_identifier_from_target(
        std::string_view("../usr/share/zoneinfo"), root);
    expect(!root_itself &&
               root_itself.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "A target naming only the root records no identifier");

    const auto outside = backend::zone_identifier_from_target(
        std::string_view("../etc/localtime.backup"), root);
    expect(!outside &&
               outside.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "A target outside any documented root records no identifier");

    const auto prefix_trap = backend::zone_identifier_from_target(
        std::string_view("/usr/share/zoneinfo-backup/x"), root);
    expect(!prefix_trap &&
               prefix_trap.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "A directory sharing the root prefix is not inside the root");

    const auto custom_root = backend::zone_identifier_from_target(
        std::string_view("../custom/zdir/Area/City"),
        std::string_view("/custom/zdir"));
    expect(custom_root && *custom_root == "Area/City",
           "A configured TZDIR root extracts its own identifiers");

    const auto trailing_root = backend::zone_identifier_from_target(
        std::string_view("../usr/share/zoneinfo/Asia/Shanghai"),
        std::string_view("/usr/share/zoneinfo/"));
    expect(trailing_root && *trailing_root == "Asia/Shanghai",
           "A trailing slash does not change the configured root");

    const auto wrong_relative_base = backend::zone_identifier_from_target(
        std::string_view("usr/share/zoneinfo/Asia/Shanghai"), root);
    expect(!wrong_relative_base &&
               wrong_relative_base.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "A relative localtime target is resolved against /etc");

    const auto empty_root = backend::zone_identifier_from_target(
        std::string_view("anything"), std::string_view());
    expect(!empty_root &&
               empty_root.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "An empty root records no identifier");

    const auto filesystem_root = backend::zone_identifier_from_target(
        std::string_view("/Area/City"), std::string_view("/"));
    expect(filesystem_root && *filesystem_root == "Area/City",
           "The filesystem root can be used as an explicit TZDIR");
}

void test_language_list_boundaries() {
    const auto ordered =
        syscape::detail::locale_common::validate_language_list(
            syscape::result<std::vector<std::string>>(
                {"zh-Hans-CN", "en"}));
    expect(ordered && ordered->size() == 2U &&
               (*ordered)[0] == "zh-Hans-CN" && (*ordered)[1] == "en",
           "Language entries are preserved verbatim in recorded order");

    const auto empty_list =
        syscape::detail::locale_common::validate_language_list(
            syscape::result<std::vector<std::string>>());
    expect(!empty_list &&
               empty_list.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty language list is malformed platform data");

    const auto empty_entry =
        syscape::detail::locale_common::validate_language_list(
            syscape::result<std::vector<std::string>>({"en", ""}));
    expect(!empty_entry &&
               empty_entry.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty entry is malformed platform data");

    const auto invalid_entry =
        syscape::detail::locale_common::validate_language_list(
            syscape::result<std::vector<std::string>>(
                {"en", std::string(1U, static_cast<char>(0xff))}));
    expect(!invalid_entry &&
               invalid_entry.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 entry must fail at the public boundary");

    const auto propagated =
        syscape::detail::locale_common::validate_language_list(
            syscape::result<std::vector<std::string>>(
                syscape::fail(syscape::errc::not_supported)));
    expect(!propagated &&
               propagated.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "Backend failures propagate through the boundary unchanged");
}

/// Independently reads the documented configuration link so that the
/// backend identifier query is cross-checked rather than re-executed.
///
/// The readlink error number is captured immediately because intervening
/// calls would clobber it.
std::string reference_localtime_target(bool& readable, int& failure_errno) {
    readable = false;
    char buffer[8192];
    const ssize_t length =
        ::readlink("/etc/localtime", buffer, sizeof(buffer));
    if (length > 0) {
        readable = true;
        return std::string(buffer, static_cast<std::size_t>(length));
    }
    failure_errno = errno;
    return std::string();
}

void test_time_zone_identifier_queries() {
    // The live queries below manipulate the environment; copy caller state
    // because setenv() may invalidate pointers returned by getenv().
    const char* const previous_tz_pointer = ::getenv("TZ");
    const bool had_previous_tz = previous_tz_pointer != nullptr;
    const std::string previous_tz =
        had_previous_tz ? std::string(previous_tz_pointer) : std::string();
    const char* const previous_tzdir_pointer = ::getenv("TZDIR");
    const bool had_previous_tzdir = previous_tzdir_pointer != nullptr;
    const std::string previous_tzdir =
        had_previous_tzdir ? std::string(previous_tzdir_pointer)
                           : std::string();

    bool readable = false;
    int failure_errno = 0;
    const std::string target =
        reference_localtime_target(readable, failure_errno);

    if (::setenv("TZ", "", 1) != 0) {
        expect(false, "The test must be able to clear TZ");
        return;
    }
    const auto system_identifier = syscape::locale::time_zone_identifier();
    expect(system_identifier && *system_identifier == "UTC",
           "An explicitly empty TZ selects UTC rather than the system zone");

    if (::unsetenv("TZ") != 0) {
        expect(false, "The test must be able to unset TZ");
    } else {
        const auto default_identifier =
            syscape::locale::time_zone_identifier();
        if (!readable) {
            if (failure_errno == EINVAL) {
                expect(!default_identifier &&
                           default_identifier.error() ==
                               syscape::make_error_code(
                                   syscape::errc::not_found),
                       "A non-link localtime configuration reports not_found");
            } else {
                expect(!default_identifier,
                       "Unreadable configurations propagate their failure");
            }
        } else {
            expect(default_identifier && !default_identifier->empty() &&
                       syscape::detail::is_valid_utf8(*default_identifier) &&
                       default_identifier->front() != '/',
                   "The system identifier is a non-empty relative path");
            static constexpr std::string_view marker = "/usr/share/zoneinfo/";
            const std::size_t position = target.find(marker);
            if (position != std::string::npos) {
                const std::string expected =
                    target.substr(position + marker.size());
                expect(default_identifier &&
                           *default_identifier == expected,
                       "The identifier matches the documented link target");
            }
        }
    }

    if (::setenv("TZ", ":Asia/Shanghai", 1) == 0 &&
        ::access("/usr/share/zoneinfo/Asia/Shanghai", F_OK) == 0) {
        const auto override_identifier =
            syscape::locale::time_zone_identifier();
        expect(override_identifier &&
                   *override_identifier == "Asia/Shanghai",
               "A file-form TZ names its zone file verbatim");

        if (::setenv("TZ", "Asia/Shanghai", 1) == 0) {
            const auto without_colon =
                syscape::locale::time_zone_identifier();
            expect(without_colon && *without_colon == "Asia/Shanghai",
                   "The optional TZ file-form colon may be omitted");
        }

        if (::setenv("TZ", ":Asia/Shanghai", 1) == 0 &&
            ::setenv("TZDIR", "/usr/share/zoneinfo/", 1) == 0) {
            const auto trailing_root =
                syscape::locale::time_zone_identifier();
            expect(trailing_root && *trailing_root == "Asia/Shanghai",
                   "A trailing slash in TZDIR preserves the identifier");
        }
    }

    if (::setenv("TZ", ":Asia", 1) == 0 &&
        ::setenv("TZDIR", "/usr/share/zoneinfo", 1) == 0 &&
        ::access("/usr/share/zoneinfo/Asia", F_OK) == 0) {
        const auto directory = syscape::locale::time_zone_identifier();
        expect(!directory &&
                   directory.error() ==
                       syscape::make_error_code(syscape::errc::malformed_data),
               "A zoneinfo directory is not accepted as a TZif file");
    }

    if (::setenv("TZ", "EST5EDT,M3.2.0,M11.1.0", 1) == 0) {
        const auto rule_identifier = syscape::locale::time_zone_identifier();
        expect(!rule_identifier &&
                   rule_identifier.error() ==
                       syscape::make_error_code(syscape::errc::not_found),
               "A POSIX rule string records no time-zone identifier");
    }

    const auto languages = syscape::locale::preferred_languages();
    expect(!languages &&
               languages.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "Linux exposes no preferred-languages source and reports it");

    const auto region = syscape::locale::country_region_code();
    expect(!region &&
               region.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "Linux exposes no separate region source and reports it");

    if (had_previous_tz) {
        ::setenv("TZ", previous_tz.c_str(), 1);
    } else {
        ::unsetenv("TZ");
    }
    if (had_previous_tzdir) {
        ::setenv("TZDIR", previous_tzdir.c_str(), 1);
    } else {
        ::unsetenv("TZDIR");
    }
}

} // namespace

int main() {
    test_offset_parsing();
    test_text_boundaries();
    test_runtime_queries();
    test_zone_identifier_extraction();
    test_language_list_boundaries();
    test_time_zone_identifier_queries();
    return failures == 0 ? 0 : 1;
}
