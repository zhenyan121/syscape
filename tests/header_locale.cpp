#include <cstdint>
#include <string>

#include <syscape/locale.hpp>
#include <syscape/locale.hpp>

int main() {
    const syscape::result<std::string> locale =
        syscape::locale::current_locale();
    const syscape::result<std::string> encoding =
        syscape::locale::text_encoding();
    const syscape::result<std::int32_t> offset =
        syscape::locale::utc_offset_seconds();

    static_cast<void>(encoding);
    static_cast<void>(offset);
    return locale && locale->empty() ? 1 : 0;
}
