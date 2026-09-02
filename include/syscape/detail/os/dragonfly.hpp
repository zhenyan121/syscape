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

inline result<std::chrono::system_clock::time_point>
timeval_to_time_point(std::int64_t seconds, std::int64_t microseconds) {
    if (microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    return timespec_to_time_point(seconds, microseconds * 1000);
}

inline result<std::chrono::milliseconds>
timespec_to_uptime(std::int64_t seconds, std::int64_t nanoseconds) {
    if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
        return fail(errc::malformed_data);
    }
    const std::int64_t maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::milliseconds::max())
            .count();
    if (seconds > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    const auto whole = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(seconds));
    const auto fraction = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::nanoseconds(nanoseconds));
    if (fraction > std::chrono::milliseconds::max() - whole) {
        return fail(errc::value_too_large);
    }
    return whole + fraction;
}

inline result<std::chrono::system_clock::time_point> boot_time_from_clocks(
    std::int64_t realtime_seconds, std::int64_t realtime_nanoseconds,
    std::int64_t uptime_seconds, std::int64_t uptime_nanoseconds) {
    if (realtime_seconds < 0 || realtime_nanoseconds < 0 ||
        realtime_nanoseconds >= 1000000000 || uptime_seconds < 0 ||
        uptime_nanoseconds < 0 || uptime_nanoseconds >= 1000000000) {
        return fail(errc::malformed_data);
    }
    if (uptime_seconds > realtime_seconds ||
        (uptime_seconds == realtime_seconds &&
         uptime_nanoseconds > realtime_nanoseconds)) {
        return fail(errc::malformed_data);
    }

    std::int64_t seconds = realtime_seconds - uptime_seconds;
    std::int64_t nanoseconds = realtime_nanoseconds - uptime_nanoseconds;
    if (nanoseconds < 0) {
        --seconds;
        nanoseconds += 1000000000;
    }
    return timespec_to_time_point(seconds, nanoseconds);
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

inline result<std::chrono::milliseconds> monotonic_uptime() {
    struct timespec value {};
    if (::clock_gettime(CLOCK_UPTIME, &value) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return timespec_to_uptime(static_cast<std::int64_t>(value.tv_sec),
                              static_cast<std::int64_t>(value.tv_nsec));
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    int name[] = {CTL_KERN, KERN_BOOTTIME};
    struct timeval value {};
    std::size_t size = sizeof(value);
    if (::sysctl(name, 2U, &value, &size, nullptr, 0U) == 0 &&
        size == sizeof(value)) {
        const auto converted =
            timeval_to_time_point(static_cast<std::int64_t>(value.tv_sec),
                                  static_cast<std::int64_t>(value.tv_usec));
        if (converted) {
            return converted;
        }
    }

    struct timespec realtime {};
    if (::clock_gettime(CLOCK_REALTIME, &realtime) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    struct timespec elapsed {};
    if (::clock_gettime(CLOCK_UPTIME, &elapsed) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return boot_time_from_clocks(static_cast<std::int64_t>(realtime.tv_sec),
                                 static_cast<std::int64_t>(realtime.tv_nsec),
                                 static_cast<std::int64_t>(elapsed.tv_sec),
                                 static_cast<std::int64_t>(elapsed.tv_nsec));
}

inline result<std::chrono::milliseconds> uptime() {
    return monotonic_uptime();
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
