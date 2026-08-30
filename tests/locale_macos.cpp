#include <cctype>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <system_error>
#include <time.h>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>

#include <syscape/detail/locale/common.hpp>
#include <syscape/detail/locale/macos.hpp>
#include <syscape/detail/locale/posix.hpp>
#include <syscape/detail/locale/preferences_macos.hpp>
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
    const auto plus_nine_thirty =
        syscape::detail::locale_backend::parse_gmt_offset_text("+0930");
    expect(plus_nine_thirty && *plus_nine_thirty == 34200,
           "A +0930 rendering must parse to 34200 seconds");

    const auto zero =
        syscape::detail::locale_backend::parse_gmt_offset_text("-0000");
    expect(zero && *zero == 0,
           "A negative-zero rendering is a valid zero offset");

    expect(!syscape::detail::locale_backend::parse_gmt_offset_text("+2400"),
           "Hours beyond 23 are malformed platform data");
}

void test_text_boundaries() {
    const auto empty =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(""));
    expect(!empty &&
               empty.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty label is rejected as malformed platform data");

    const auto invalid =
        syscape::detail::locale_common::validate_utf8_label(
            syscape::result<std::string>(std::string("/tmp/\xffx")));
    expect(!invalid &&
               invalid.error() == syscape::errc::invalid_encoding,
           "A non-UTF-8 label must fail at the public boundary");

    const auto day_offset =
        syscape::detail::locale_common::validate_utc_offset_seconds(
            syscape::result<std::int32_t>(90000));
    expect(!day_offset &&
               day_offset.error() == syscape::errc::malformed_data,
           "An out-of-day-range offset is malformed platform data");
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
    const auto locale = syscape::locale::current_locale();
    expect(locale && !locale->empty() &&
               syscape::detail::is_valid_utf8(*locale),
           "macOS must report the C runtime's locale string verbatim");

    const auto encoding = syscape::locale::text_encoding();
    expect(encoding && !encoding->empty() &&
               syscape::detail::is_valid_utf8(*encoding),
           "macOS must report the LC_CTYPE codeset name verbatim");

    ::tzset();
    const std::time_t now = ::time(nullptr);
    std::tm local {};
    expect(::localtime_r(&now, &local) != nullptr,
           "The localtime_r reference lookup must not fail natively");

    long reference_seconds = 0;
    const auto offset = syscape::locale::utc_offset_seconds();
    if (reference_offset(local, reference_seconds)) {
        expect(offset &&
                   *offset == static_cast<std::int32_t>(reference_seconds),
               "macOS must report the current UTC offset from the zone "
               "rules");
    } else {
        expect(!offset &&
                   offset.error() ==
                       syscape::make_error_code(syscape::errc::not_found),
               "An indeterminable reference zone must report not_found");
    }
}

/// Creates an owned CFString from a literal for synthetic records.
///
/// Ownership transfers to the caller, matching the injected API results
/// that the collectors release exactly once.
::CFStringRef make_cf_string(const char* text) {
    return ::CFStringCreateWithCString(
        nullptr, text, ::kCFStringEncodingUTF8);
}

void test_language_collection() {
    namespace backend = syscape::detail::locale_backend;

    struct ordered_api {
        static syscape::result<::CFArrayRef> preferred_languages() {
            ::CFStringRef entries[2] = {make_cf_string("zh-Hans-CN"),
                                        make_cf_string("en-US")};
            ::CFArrayRef array = ::CFArrayCreate(
                nullptr, reinterpret_cast<const void**>(entries), 2,
                &::kCFTypeArrayCallBacks);
            ::CFRelease(entries[0]);
            ::CFRelease(entries[1]);
            if (array == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return array;
        }
    };
    const auto collected =
        backend::collect_preferred_languages<ordered_api>();
    expect(collected && collected->size() == 2U &&
               (*collected)[0] == "zh-Hans-CN" && (*collected)[1] == "en-US",
           "Language entries are preserved verbatim in recorded order");

    struct typed_wrong_api {
        static syscape::result<::CFArrayRef> preferred_languages() {
            const int value = 42;
            ::CFNumberRef number =
                ::CFNumberCreate(nullptr, ::kCFNumberIntType, &value);
            ::CFArrayRef array = ::CFArrayCreate(
                nullptr, reinterpret_cast<const void**>(&number), 1,
                &::kCFTypeArrayCallBacks);
            ::CFRelease(number);
            if (array == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return array;
        }
    };
    const auto wrong_type =
        backend::collect_preferred_languages<typed_wrong_api>();
    expect(!wrong_type &&
               wrong_type.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A non-string language entry is malformed platform data");

    struct empty_api {
        static syscape::result<::CFArrayRef> preferred_languages() {
            ::CFArrayRef array = ::CFArrayCreate(
                nullptr, nullptr, 0, &::kCFTypeArrayCallBacks);
            if (array == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return array;
        }
    };
    const auto empty = backend::collect_preferred_languages<empty_api>();
    expect(empty && empty->empty(),
           "The backend copies an empty recording verbatim");
    const auto boundary =
        syscape::detail::locale_common::validate_language_list(empty);
    expect(!boundary &&
               boundary.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "An empty recorded list is rejected at the public boundary");
}

void test_single_string_fact_collection() {
    namespace backend = syscape::detail::locale_backend;

    struct string_values_api {
        static syscape::result<::CFTypeRef> region_value() {
            ::CFStringRef value = make_cf_string("CN");
            if (value == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return value;
        }
        static syscape::result<::CFTypeRef> time_zone_name() {
            ::CFStringRef value = make_cf_string("Asia/Shanghai");
            if (value == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return value;
        }
    };

    const auto region =
        backend::collect_country_region_code<string_values_api>();
    expect(region && *region == "CN",
           "A recorded region code is copied verbatim");

    const auto zone =
        backend::collect_time_zone_identifier<string_values_api>();
    expect(zone && *zone == "Asia/Shanghai",
           "A recorded time-zone identifier is copied verbatim");

    struct absent_region_api {
        static syscape::result<::CFTypeRef> region_value() {
            return syscape::fail(syscape::errc::not_found);
        }
        static syscape::result<::CFTypeRef> time_zone_name() {
            return syscape::fail(syscape::errc::not_found);
        }
    };
    const auto absent =
        backend::collect_country_region_code<absent_region_api>();
    expect(!absent &&
               absent.error() ==
                   syscape::make_error_code(syscape::errc::not_found),
           "An absent region record reports not_found");

    struct wrong_typed_api {
        static syscape::result<::CFTypeRef> region_value() {
            const int value = 156;
            ::CFNumberRef number =
                ::CFNumberCreate(nullptr, ::kCFNumberIntType, &value);
            if (number == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return number;
        }
        static syscape::result<::CFTypeRef> time_zone_name() {
            const int value = 42;
            ::CFNumberRef number =
                ::CFNumberCreate(nullptr, ::kCFNumberIntType, &value);
            if (number == nullptr) { return syscape::fail(syscape::errc::io_error); }
            return number;
        }
    };
    const auto wrong_region =
        backend::collect_country_region_code<wrong_typed_api>();
    expect(!wrong_region &&
               wrong_region.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A non-string region record is malformed platform data");
    const auto wrong_zone =
        backend::collect_time_zone_identifier<wrong_typed_api>();
    expect(!wrong_zone &&
               wrong_zone.error() ==
                   syscape::make_error_code(syscape::errc::malformed_data),
           "A non-string zone name is malformed platform data");
}

void test_runtime_preference_queries() {
    const auto languages = syscape::locale::preferred_languages();
    expect(languages && !languages->empty(),
           "macOS must report the user's language preference list");
    if (languages) {
        for (const std::string& language : *languages) {
            expect(!language.empty() &&
                       syscape::detail::is_valid_utf8(language),
                   "Every reported language entry is non-empty UTF-8");
        }
    }

    const auto region = syscape::locale::country_region_code();
    expect(region && !region->empty() &&
               syscape::detail::is_valid_utf8(*region),
           "macOS must report a region code from the current locale");

    const auto zone = syscape::locale::time_zone_identifier();
    expect(!zone &&
               zone.error() ==
                   syscape::make_error_code(syscape::errc::not_supported),
           "An ambiguous CoreFoundation GMT fallback is not fabricated as "
           "a configured identifier");
}

} // namespace

int main() {
    test_offset_parsing();
    test_text_boundaries();
    test_runtime_queries();
    test_language_collection();
    test_single_string_fact_collection();
    test_runtime_preference_queries();
    return failures == 0 ? 0 : 1;
}
