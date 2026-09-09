#ifndef SYSCAPE_DETAIL_LOCALE_NUTTX_HPP
#define SYSCAPE_DETAIL_LOCALE_NUTTX_HPP

#include <cerrno>
#include <clocale>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <system_error>
#include <vector>
#if !defined(__NuttX__) || defined(CONFIG_LIBC_LOCALE)
#include <langinfo.h>
#endif

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline result<std::string> current_locale() {
#if defined(__NuttX__) && !defined(CONFIG_LIBC_LOCALE)
    return fail(errc::not_supported);
#else
    const char* value = std::setlocale(LC_ALL, nullptr);
    return value != nullptr ? result<std::string>(std::string(value))
                            : fail(errc::malformed_data);
#endif
}

inline result<std::string> text_encoding() {
#if defined(__NuttX__) && !defined(CONFIG_LIBC_LOCALE)
    return fail(errc::not_supported);
#else
    const char* value = ::nl_langinfo(CODESET);
    return value != nullptr ? result<std::string>(std::string(value))
                            : fail(errc::malformed_data);
#endif
}

inline result<std::int32_t> utc_offset_seconds() {
#if defined(__NuttX__) && !defined(CONFIG_LIBC_LOCALTIME)
    return fail(errc::not_supported);
#else
    ::tzset();
    const std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        return fail(std::error_code(EOVERFLOW, std::generic_category()));
    }
    std::tm local {};
    errno = 0;
    if (::localtime_r(&now, &local) == nullptr) {
        const int error = errno;
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    constexpr long seconds_per_day = 24 * 60 * 60;
    if (local.tm_gmtoff <= -seconds_per_day ||
        local.tm_gmtoff >= seconds_per_day) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::int32_t>(local.tm_gmtoff);
#endif
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
