#ifndef SYSCAPE_DETAIL_OS_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_OS_APPLE_MOBILE_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <string>
#include <system_error>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/config.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    value.resize(size);
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n')) {
        value.pop_back();
    }
    return value.empty() ? result<std::string>(fail(errc::not_found))
                         : result<std::string>(std::move(value));
}

inline result<std::string> product_name() {
#if defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST
    return std::string("macOS");
#elif defined(TARGET_OS_VISION) && TARGET_OS_VISION
    return std::string("visionOS");
#elif defined(TARGET_OS_WATCH) && TARGET_OS_WATCH
    return std::string("watchOS");
#elif defined(TARGET_OS_TV) && TARGET_OS_TV
    return std::string("tvOS");
#elif defined(TARGET_OS_IOS) && TARGET_OS_IOS
    return std::string("iOS");
#elif defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
    return std::string("iOS");
#else
    return std::string("iOS");
#endif
}

inline result<std::string> product_version() {
    return sysctl_string("kern.osproductversion");
}

inline result<std::string> build_identifier() {
    return sysctl_string("kern.osversion");
}

inline result<std::string> kernel_name() {
    return sysctl_string("kern.ostype");
}

inline result<std::string> kernel_version() {
    return sysctl_string("kern.osrelease");
}

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    while (buffer.size() <= maximum_size) {
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') {
                ++end;
            }
            if (end < buffer.size()) {
                return std::string(buffer.data(), end);
            }
            buffer.resize(buffer.size() * 2U);
            continue;
        }
        if (errno != ENAMETOOLONG && errno != EINVAL) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        buffer.resize(buffer.size() * 2U);
    }
    return fail(errc::value_too_large);
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    int name[] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval value {};
    std::size_t size = sizeof(value);
    if (::sysctl(name, 2U, &value, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(value) || value.tv_sec <= 0 || value.tv_usec < 0 ||
        value.tv_usec >= 1000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const auto max_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (value.tv_sec > max_seconds) {
        return fail(errc::value_too_large);
    }
    const auto whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(value.tv_sec));
    const auto fraction = std::chrono::duration_cast<clock::duration>(
        std::chrono::microseconds(value.tv_usec));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline result<std::chrono::milliseconds> uptime() {
    const result<std::chrono::system_clock::time_point> started = boot_time();
    if (!started) {
        return fail(started.error());
    }
    const auto elapsed = std::chrono::system_clock::now() - *started;
    if (elapsed < std::chrono::system_clock::duration::zero()) {
        return fail(errc::malformed_data);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
}

inline result<std::string> boot_identifier() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
