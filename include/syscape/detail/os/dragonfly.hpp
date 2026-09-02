#ifndef SYSCAPE_DETAIL_OS_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_OS_DRAGONFLY_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/types.h>
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

inline result<std::chrono::system_clock::time_point>
timespec_to_time_point(std::int64_t seconds, std::int64_t nanoseconds) {
    if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const std::int64_t maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    const clock::duration whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds));
    const clock::duration fraction =
        std::chrono::duration_cast<clock::duration>(
            std::chrono::nanoseconds(nanoseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline result<std::string> sysctl_string(const char* name) {
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return fail(errc::not_found);
        }
        std::string value(size, '\0');
        if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size > value.size()) {
            continue;
        }
        value.resize(size);
        while (!value.empty() && value.back() == '\0') {
            value.pop_back();
        }
        return value.empty() ? result<std::string>(fail(errc::not_found))
                             : result<std::string>(std::move(value));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::string> product_name() {
    return std::string("DragonFly BSD");
}

inline result<std::string> product_version() {
    return sysctl_string("kern.osrelease");
}

inline result<std::string> build_identifier() {
    const result<std::string> build_id = sysctl_string("kern.build_id");
    if (build_id) {
        return build_id;
    }
    if (build_id.error() != errc::not_supported &&
        build_id.error() != errc::not_found) {
        return fail(build_id.error());
    }

    const result<std::string> ident = sysctl_string("kern.ident");
    if (ident) {
        return ident;
    }
    if (ident.error() != errc::not_supported &&
        ident.error() != errc::not_found) {
        return fail(ident.error());
    }
    return sysctl_string("kern.version");
}

inline result<std::string> kernel_name() {
    return sysctl_string("kern.ostype");
}

inline result<std::string> kernel_version() {
    return sysctl_string("kern.osrelease");
}

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    while (buffer.size() <= 1024U * 1024U) {
        if (::gethostname(buffer.data(), buffer.size()) == 0) {
            std::size_t end = 0U;
            while (end < buffer.size() && buffer[end] != '\0') {
                ++end;
            }
            if (end < buffer.size()) {
                if (end == 0U) {
                    return fail(errc::not_found);
                }
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
    struct timespec value {};
    std::size_t size = sizeof(value);
    if (::sysctl(name, 2U, &value, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(value) || value.tv_sec < 0 || value.tv_nsec < 0 ||
        value.tv_nsec >= 1000000000) {
        return fail(errc::malformed_data);
    }
    return timespec_to_time_point(static_cast<std::int64_t>(value.tv_sec),
                                  static_cast<std::int64_t>(value.tv_nsec));
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
    const auto hostuuid = sysctl_string("kern.hostuuid");
    if (hostuuid) {
        return hostuuid;
    }
    const auto hwuuid = sysctl_string("hw.uuid");
    if (hwuuid) {
        return hwuuid;
    }
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
