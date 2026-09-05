#include <cstdlib>
#include <iostream>
#include <string>
#include <unistd.h>

#include <syscape/locale.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_locale_queries() {
    const auto enc = syscape::locale::text_encoding();
    expect(enc && !enc->empty(), "text encoding query must succeed");

    const auto offset = syscape::locale::utc_offset_seconds();
    expect(offset.has_value(), "utc offset query must succeed");

    const auto tz = syscape::locale::time_zone_identifier();
    expect(tz.has_value() || tz.error() == syscape::errc::not_found ||
               tz.error() == syscape::errc::not_supported ||
               tz.error() == syscape::errc::permission_denied ||
               tz.error() == std::errc::permission_denied ||
               tz.error() == std::errc::operation_not_permitted,
           "time zone query must succeed or report a documented error");

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    const char* const original_tz = ::getenv("TZ");
    const bool had_original_tz = original_tz != nullptr;
    const std::string saved_tz = had_original_tz ? original_tz : "";

    ::setenv("TZ", "Asia/Tokyo", 1);
    const auto overridden = syscape::locale::time_zone_identifier();
    expect(overridden && *overridden == "Asia/Tokyo",
           "TZ must override the system localtime link");

    ::setenv("TZ", "EST5EDT", 1);
    expect(syscape::locale::time_zone_identifier().error() ==
               syscape::errc::not_found,
           "a POSIX TZ rule must not be reported as an identifier");

    ::setenv("TZ", "", 1);
    const auto empty_override = syscape::locale::time_zone_identifier();
    expect(empty_override && *empty_override == "UTC",
           "an explicitly empty TZ must select UTC");

    if (had_original_tz) {
        ::setenv("TZ", saved_tz.c_str(), 1);
    } else {
        ::unsetenv("TZ");
    }

    char directory[] = "/tmp/syscape-time-zone-XXXXXX";
    char* const created_directory = ::mkdtemp(directory);
    expect(created_directory != nullptr,
           "time-zone fixture directory must be created");
    if (created_directory != nullptr) {
        const std::string identifier(600U, 'A');
        const std::string target = "/usr/share/lib/zoneinfo/" + identifier;
        const std::string link_path =
            std::string(created_directory) + "/localtime";
        const int linked = ::symlink(target.c_str(), link_path.c_str());
        expect(linked == 0, "long time-zone symlink must be created");
        if (linked == 0) {
            const auto parsed =
                syscape::detail::locale_backend::zoneinfo_identifier_from_link(
                    link_path.c_str());
            expect(parsed && *parsed == identifier,
                   "long time-zone symlinks must not be truncated");
            ::unlink(link_path.c_str());
        }
        ::rmdir(created_directory);
    }
#endif
}

} // namespace

int main() {
    test_locale_queries();
    return failures == 0 ? 0 : 1;
}
