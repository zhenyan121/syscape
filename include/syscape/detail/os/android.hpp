#ifndef SYSCAPE_DETAIL_OS_ANDROID_HPP
#define SYSCAPE_DETAIL_OS_ANDROID_HPP

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <syscape/detail/android/file.hpp>
#include <syscape/detail/android/property.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<struct utsname> uname_information() {
    struct utsname value {};
    if (::uname(&value) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<std::string> product_name() {
    return std::string("Android");
}

inline result<std::string> product_version() {
    result<std::string> version =
        android::get_property("ro.build.version.release");
    if (version) {
        return version;
    }
    return android::get_property("ro.build.version.sdk");
}

inline result<std::string> build_identifier() {
    result<std::string> build_id = android::get_property("ro.build.id");
    if (build_id) {
        return build_id;
    }
    build_id = android::get_property("ro.build.display.id");
    if (build_id) {
        return build_id;
    }
    return fail(errc::not_found);
}

inline result<std::string> kernel_name() {
    const result<struct utsname> value = uname_information();
    return value ? result<std::string>(std::string(value->sysname))
                 : result<std::string>(fail(value.error()));
}

inline result<std::string> kernel_version() {
    const result<struct utsname> value = uname_information();
    return value ? result<std::string>(std::string(value->release))
                 : result<std::string>(fail(value.error()));
}

inline result<std::string> host_name() {
    std::vector<char> buffer(256U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    while (buffer.size() <= maximum_size) {
        errno = 0;
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

inline result<std::string> boot_identifier() {
    result<std::string> value =
        android::read_text_file("/proc/sys/kernel/random/boot_id");
    if (value) {
        android::trim_line_end(*value);
        if (value->empty()) {
            return fail(errc::malformed_data);
        }
        return value;
    }
    if (value.error() == errc::not_found) {
        return fail(errc::not_supported);
    }
    return fail(value.error());
}

inline result<std::chrono::milliseconds> uptime() {
    struct timespec value {};
#if defined(CLOCK_BOOTTIME)
    constexpr clockid_t clock = CLOCK_BOOTTIME;
#else
    constexpr clockid_t clock = CLOCK_MONOTONIC;
#endif
    if (::clock_gettime(clock, &value) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (value.tv_sec < 0 || value.tv_nsec < 0 || value.tv_nsec >= 1000000000L) {
        return fail(errc::malformed_data);
    }
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::milliseconds::max())
            .count();
    if (value.tv_sec > maximum_seconds) {
        return fail(errc::value_too_large);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::seconds(value.tv_sec) +
        std::chrono::nanoseconds(value.tv_nsec));
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const result<std::string> statistics =
        android::read_text_file("/proc/stat");
    if (statistics) {
        std::size_t offset = 0U;
        while (offset < statistics->size()) {
            const std::size_t end = statistics->find('\n', offset);
            const std::string_view line(statistics->data() + offset,
                                        end == std::string::npos
                                            ? statistics->size() - offset
                                            : end - offset);
            if (line.size() > 6U && line.substr(0U, 6U) == "btime ") {
                long long seconds_since_epoch = 0;
                const char* first = line.data() + 6U;
                const char* last = line.data() + line.size();
                const std::from_chars_result parsed =
                    std::from_chars(first, last, seconds_since_epoch);
                if (parsed.ec != std::errc() || parsed.ptr != last ||
                    seconds_since_epoch < 0) {
                    return fail(errc::malformed_data);
                }
                using clock = std::chrono::system_clock;
                const auto maximum =
                    std::chrono::duration_cast<std::chrono::seconds>(
                        clock::duration::max())
                        .count();
                if (seconds_since_epoch > maximum) {
                    return fail(errc::value_too_large);
                }
                return clock::time_point(
                    std::chrono::duration_cast<clock::duration>(
                        std::chrono::seconds(seconds_since_epoch)));
            }
            if (end == std::string::npos) {
                break;
            }
            offset = end + 1U;
        }
    }

    // On modern Android where /proc/stat is SELinux-restricted for apps,
    // calculate boot_time from current system_clock minus uptime.
    const result<std::chrono::milliseconds> up = uptime();
    if (!up) {
        return fail(up.error());
    }
    const auto now = std::chrono::system_clock::now();
    return now - *up;
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
