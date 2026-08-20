#ifndef SYSCAPE_DETAIL_OS_MACOS_HPP
#define SYSCAPE_DETAIL_OS_MACOS_HPP

#include <cerrno>
#include <chrono>
#include <string>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) { return fail(errc::not_found); }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    value.resize(size);
    while (!value.empty() && value.back() == '\0') { value.pop_back(); }
    return value.empty() ? result<std::string>(fail(errc::not_found))
                         : result<std::string>(std::move(value));
}

inline result<std::string> product_name() { return std::string("macOS"); }
inline result<std::string> product_version() {
    return sysctl_string("kern.osproductversion");
}
inline result<std::string> build_identifier() { return sysctl_string("kern.osversion"); }
inline result<std::string> kernel_name() { return sysctl_string("kern.ostype"); }
inline result<std::string> kernel_version() { return sysctl_string("kern.osrelease"); }

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    while (buffer.size() <= 1024U * 1024U) {
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') { ++end; }
            if (end < buffer.size()) { return std::string(buffer.data(), end); }
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
    using clock = std::chrono::system_clock;
    return clock::time_point(std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(value.tv_sec) + std::chrono::microseconds(value.tv_usec)));
}

inline result<std::chrono::milliseconds> uptime() {
    const result<std::chrono::system_clock::time_point> started = boot_time();
    if (!started) { return fail(started.error()); }
    const auto elapsed = std::chrono::system_clock::now() - *started;
    if (elapsed < std::chrono::system_clock::duration::zero()) {
        return fail(errc::malformed_data);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(elapsed);
}

inline result<std::string> boot_identifier() { return fail(errc::not_supported); }

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
