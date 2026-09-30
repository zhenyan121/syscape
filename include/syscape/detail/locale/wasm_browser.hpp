#ifndef SYSCAPE_DETAIL_LOCALE_WASM_BROWSER_HPP
#define SYSCAPE_DETAIL_LOCALE_WASM_BROWSER_HPP

#include <clocale>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__has_include)
#if __has_include(<langinfo.h>) && !defined(_WIN32)
#include <langinfo.h>
#define SYSCAPE_DETAIL_LOCALE_HAS_LANGINFO_H 1
#endif
#elif !defined(_WIN32)
#include <langinfo.h>
#define SYSCAPE_DETAIL_LOCALE_HAS_LANGINFO_H 1
#endif

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline result<std::string> current_locale() {
    const char* const name = std::setlocale(LC_CTYPE, nullptr);
    if (name == nullptr) {
        return fail(errc::not_supported);
    }
    if (name[0] == '\0') {
        return fail(errc::malformed_data);
    }
    return std::string(name);
}

inline result<std::string> text_encoding() {
#if defined(SYSCAPE_DETAIL_LOCALE_HAS_LANGINFO_H) &&                           \
    (!defined(__ANDROID__) || __ANDROID_API__ >= 26)
    const char* const codeset = ::nl_langinfo(CODESET);
    if (codeset == nullptr) {
        return fail(errc::not_supported);
    }
    if (codeset[0] == '\0') {
        return fail(errc::malformed_data);
    }
    return std::string(codeset);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::int32_t> utc_offset_seconds() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

inline result<std::string> time_zone_identifier() {
    return fail(errc::not_supported);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
