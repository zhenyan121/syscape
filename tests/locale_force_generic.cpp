#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/locale.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::locale::current_locale()) &&
                   unsupported(syscape::locale::text_encoding()) &&
                   unsupported(syscape::locale::utc_offset_seconds()) &&
                   unsupported(syscape::locale::preferred_languages()) &&
                   unsupported(syscape::locale::country_region_code()) &&
                   unsupported(syscape::locale::time_zone_identifier())
               ? 0
               : 1;
}
