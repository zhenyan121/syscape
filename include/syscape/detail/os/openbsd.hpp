#ifndef SYSCAPE_DETAIL_OS_OPENBSD_HPP
#define SYSCAPE_DETAIL_OS_OPENBSD_HPP

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
timeval_to_time_point(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
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
            std::chrono::microseconds(microseconds));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
}

inline result<std::string> sysctl_mib_string(const int* mib,
                                             unsigned int mib_len) {
    constexpr int maximum_attempts = 4;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        int mib_copy[8];
        if (mib_len > 8U) {
            return fail(errc::not_supported);
        }
        for (unsigned int i = 0; i < mib_len; ++i) {
            mib_copy[i] = mib[i];
        }
        if (::sysctl(mib_copy, mib_len, nullptr, &size, nullptr, 0U) != 0) {
            if (errno == ENOENT) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (size == 0U) {
            return fail(errc::not_found);
        }
        std::string value(size, '\0');
        if (::sysctl(mib_copy, mib_len, &value[0], &size, nullptr, 0U) != 0) {
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
    return std::string("OpenBSD");
}

inline result<std::string> product_version() {
    int mib[] = {CTL_KERN, KERN_OSRELEASE};
    return sysctl_mib_string(mib, 2U);
}

inline result<std::string> build_identifier() {
    int mib[] = {CTL_KERN, KERN_VERSION};
    return sysctl_mib_string(mib, 2U);
}

inline result<std::string> kernel_name() {
    int mib[] = {CTL_KERN, KERN_OSTYPE};
    return sysctl_mib_string(mib, 2U);
}

inline result<std::string> kernel_version() {
    int mib[] = {CTL_KERN, KERN_OSRELEASE};
    return sysctl_mib_string(mib, 2U);
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
    if (size != sizeof(value) || value.tv_sec < 0 || value.tv_usec < 0 ||
        value.tv_usec >= 1000000) {
        return fail(errc::malformed_data);
    }
    return timeval_to_time_point(static_cast<std::int64_t>(value.tv_sec),
                                 static_cast<std::int64_t>(value.tv_usec));
}

inline result<std::chrono::milliseconds> uptime() {
#if defined(CLOCK_UPTIME)
    struct timespec ts {};
    if (::clock_gettime(CLOCK_UPTIME, &ts) == 0) {
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
            return fail(errc::malformed_data);
        }
        constexpr std::int64_t max_sec =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::milliseconds::max())
                .count();
        if (ts.tv_sec > max_sec) {
            return fail(errc::value_too_large);
        }
        const auto ms = std::chrono::seconds(ts.tv_sec) +
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::nanoseconds(ts.tv_nsec));
        return std::chrono::duration_cast<std::chrono::milliseconds>(ms);
    }
#elif defined(CLOCK_BOOTTIME)
    struct timespec ts {};
    if (::clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        if (ts.tv_sec < 0 || ts.tv_nsec < 0 || ts.tv_nsec >= 1000000000) {
            return fail(errc::malformed_data);
        }
        constexpr std::int64_t max_sec =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::milliseconds::max())
                .count();
        if (ts.tv_sec > max_sec) {
            return fail(errc::value_too_large);
        }
        const auto ms = std::chrono::seconds(ts.tv_sec) +
                        std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::nanoseconds(ts.tv_nsec));
        return std::chrono::duration_cast<std::chrono::milliseconds>(ms);
    }
#endif
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
