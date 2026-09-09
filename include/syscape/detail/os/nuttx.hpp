#ifndef SYSCAPE_DETAIL_OS_NUTTX_HPP
#define SYSCAPE_DETAIL_OS_NUTTX_HPP

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <string>
#include <system_error>
#include <time.h>
#include <sys/utsname.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> nuttx_uts_field(const char* value) {
    if (value == nullptr || value[0] == '\0') {
        return fail(errc::not_found);
    }
    std::string text(value);
    if (!is_valid_utf8(text)) {
        return fail(errc::invalid_encoding);
    }
    return text;
}

inline result<struct ::utsname> nuttx_uname() {
    struct ::utsname value {};
    errno = 0;
    if (::uname(&value) == 0) {
        return value;
    }
    const int error = errno;
    if (error == EACCES || error == EPERM) {
        return fail(errc::permission_denied);
    }
    return error != 0 ? fail(std::error_code(error, std::generic_category()))
                      : fail(errc::io_error);
}

inline result<std::string> product_name() {
    return std::string("NuttX");
}

inline result<std::string> product_version() {
    const auto value = nuttx_uname();
    return value ? nuttx_uts_field(value->release) : fail(value.error());
}

inline result<std::string> build_identifier() {
    const auto value = nuttx_uname();
    return value ? nuttx_uts_field(value->version) : fail(value.error());
}

inline result<std::string> kernel_name() {
    const auto value = nuttx_uname();
    return value ? nuttx_uts_field(value->sysname) : fail(value.error());
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    const auto value = nuttx_uname();
    return value ? nuttx_uts_field(value->nodename) : fail(value.error());
}

inline result<std::chrono::milliseconds> uptime() {
    struct ::timespec value {};
    errno = 0;
    if (::clock_gettime(CLOCK_MONOTONIC, &value) != 0) {
        const int error = errno;
        if (error == EINVAL || error == ENOSYS) {
            return fail(errc::not_supported);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    if (value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) {
        return fail(errc::malformed_data);
    }
    const auto seconds = static_cast<std::uint64_t>(value.tv_sec);
    const auto maximum =
        static_cast<std::uint64_t>((std::chrono::milliseconds::max)().count());
    if (seconds > maximum / 1000U) {
        return fail(errc::value_too_large);
    }
    const auto milliseconds =
        seconds * 1000U + static_cast<std::uint64_t>(value.tv_nsec / 1000000L);
    if (milliseconds > maximum) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(milliseconds));
}

// CLOCK_REALTIME may intentionally be left at the Unix epoch until an RTC or
// network time source initializes it, so deriving a boot instant is unsafe.
inline result<std::chrono::system_clock::time_point> boot_time() {
    return fail(errc::not_supported);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
