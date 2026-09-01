#ifndef SYSCAPE_DETAIL_ANDROID_PROPERTY_HPP
#define SYSCAPE_DETAIL_ANDROID_PROPERTY_HPP

#include <cstddef>
#include <string>
#include <system_error>

#if defined(__ANDROID__)
#if defined(__has_include)
#if __has_include(<sys/system_properties.h>)
#include <sys/system_properties.h>
#else
extern "C" int __system_property_get(const char* name, char* value);
#endif
#else
#include <sys/system_properties.h>
#endif
#endif

#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace android {

#ifndef PROP_VALUE_MAX
#define PROP_VALUE_MAX 92
#endif

inline result<std::string> get_property(const char* name) {
#if defined(__ANDROID__)
    char value[PROP_VALUE_MAX];
    const int length = ::__system_property_get(name, value);
    if (length <= 0) {
        return fail(errc::not_found);
    }
    return std::string(value, static_cast<std::size_t>(length));
#else
    static_cast<void>(name);
    return fail(errc::not_supported);
#endif
}

inline std::string get_property_or(const char* name,
                                   const std::string& fallback) {
#if defined(__ANDROID__)
    char value[PROP_VALUE_MAX];
    const int length = ::__system_property_get(name, value);
    if (length > 0) {
        return std::string(value, static_cast<std::size_t>(length));
    }
#else
    static_cast<void>(name);
#endif
    return fallback;
}

} // namespace android
} // namespace detail
} // namespace syscape

#endif
