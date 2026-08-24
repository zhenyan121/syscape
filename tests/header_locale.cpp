#include <cstdint>
#include <string>
#include <vector>

#include <syscape/locale.hpp>
#include <syscape/locale.hpp>

int main() {
    const syscape::result<std::string> locale =
        syscape::locale::current_locale();
    const syscape::result<std::string> encoding =
        syscape::locale::text_encoding();
    const syscape::result<std::int32_t> offset =
        syscape::locale::utc_offset_seconds();
    const syscape::result<std::vector<std::string>> languages =
        syscape::locale::preferred_languages();
    const syscape::result<std::string> region =
        syscape::locale::country_region_code();
    const syscape::result<std::string> zone =
        syscape::locale::time_zone_identifier();

    static_cast<void>(encoding);
    static_cast<void>(offset);
    static_cast<void>(languages);
    static_cast<void>(region);
    static_cast<void>(zone);
    return locale && locale->empty() ? 1 : 0;
}
