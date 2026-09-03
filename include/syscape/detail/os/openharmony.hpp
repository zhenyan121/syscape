#ifndef SYSCAPE_DETAIL_OS_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_OS_OPENHARMONY_HPP

#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/openharmony/parameter.hpp>
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
    const auto dist = openharmony::distribution_os_name();
    if (dist && !dist->empty()) {
        return dist;
    }
    if (!dist && dist.error() != errc::not_found &&
        dist.error() != errc::not_supported) {
        return fail(dist.error());
    }

    const auto os_name = openharmony::os_full_name();
    if (os_name && !os_name->empty()) {
        if (os_name->find("HarmonyOS") != std::string::npos) {
            return std::string("HarmonyOS");
        }
        if (os_name->find("OpenHarmony") != std::string::npos) {
            return std::string("OpenHarmony");
        }
    }
    if (!os_name && os_name.error() != errc::not_found &&
        os_name.error() != errc::not_supported) {
        return fail(os_name.error());
    }

    return fail(errc::not_found);
}

inline result<std::string> product_version() {
    result<std::string> version = openharmony::distribution_os_version();
    if (version && !version->empty()) {
        return version;
    }
    if (!version && version.error() != errc::not_found &&
        version.error() != errc::not_supported) {
        return fail(version.error());
    }

    version = openharmony::display_version();
    if (version && !version->empty()) {
        return version;
    }
    if (!version && version.error() != errc::not_found &&
        version.error() != errc::not_supported) {
        return fail(version.error());
    }

    version = openharmony::get_parameter("const.ohos.version.release");
    if (version && !version->empty()) {
        return version;
    }
    if (!version && version.error() != errc::not_found &&
        version.error() != errc::not_supported) {
        return fail(version.error());
    }

    version = openharmony::get_parameter("const.build.os.version");
    if (version && !version->empty()) {
        return version;
    }
    if (!version && version.error() != errc::not_found &&
        version.error() != errc::not_supported) {
        return fail(version.error());
    }

    return fail(errc::not_found);
}

inline result<std::string> build_identifier() {
    result<std::string> build_id = openharmony::get_parameter("const.build.id");
    if (build_id && !build_id->empty()) {
        return build_id;
    }
    if (!build_id && build_id.error() != errc::not_found &&
        build_id.error() != errc::not_supported) {
        return fail(build_id.error());
    }

    build_id = openharmony::incremental_version();
    if (build_id && !build_id->empty()) {
        return build_id;
    }
    if (!build_id && build_id.error() != errc::not_found &&
        build_id.error() != errc::not_supported) {
        return fail(build_id.error());
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
        openharmony::read_text_file("/proc/sys/kernel/random/boot_id");
    if (value) {
        openharmony::trim_line_end(*value);
        if (value->empty()) {
            return fail(errc::not_found);
        }
        return value;
    }
    return fail(value.error());
}

inline result<std::chrono::milliseconds> monotonic_uptime() {
    struct timespec ts {};
#if defined(CLOCK_BOOTTIME)
    if (::clock_gettime(CLOCK_BOOTTIME, &ts) == 0) {
        const auto seconds = static_cast<std::int64_t>(ts.tv_sec);
        const auto nanoseconds = static_cast<std::int64_t>(ts.tv_nsec);
        return std::chrono::milliseconds(seconds * 1000 +
                                         nanoseconds / 1000000);
    }
#endif
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        const auto seconds = static_cast<std::int64_t>(ts.tv_sec);
        const auto nanoseconds = static_cast<std::int64_t>(ts.tv_nsec);
        return std::chrono::milliseconds(seconds * 1000 +
                                         nanoseconds / 1000000);
    }
    return fail(std::error_code(errno, std::generic_category()));
}

inline result<std::chrono::milliseconds>
parse_proc_uptime(std::string_view text) {
    std::string_view view = text;
    openharmony::strip_trailing_newlines(view);
    while (!view.empty() && (view.front() == ' ' || view.front() == '\t')) {
        view.remove_prefix(1U);
    }
    if (view.empty()) {
        return fail(errc::malformed_data);
    }
    const std::size_t space = view.find_first_of(" \t");
    const std::string_view token_view =
        (space != std::string_view::npos) ? view.substr(0U, space) : view;
    if (token_view.empty()) {
        return fail(errc::malformed_data);
    }
    const std::string token(token_view);
    char* end = nullptr;
    errno = 0;
    const double seconds = std::strtod(token.c_str(), &end);
    if (errno == ERANGE || !std::isfinite(seconds)) {
        return fail(errc::value_too_large);
    }
    if (end != token.c_str() + token.size() || seconds < 0.0) {
        return fail(errc::malformed_data);
    }
    constexpr double max_seconds =
        static_cast<double>((std::numeric_limits<std::int64_t>::max)() / 1000);
    if (seconds > max_seconds) {
        return fail(errc::value_too_large);
    }
    return std::chrono::milliseconds(
        static_cast<std::int64_t>(seconds * 1000.0));
}

inline result<std::chrono::milliseconds> uptime() {
    const result<std::string> contents =
        openharmony::read_text_file("/proc/uptime", 256U);
    if (contents) {
        return parse_proc_uptime(*contents);
    }
    if (contents.error() == errc::not_found) {
        return monotonic_uptime();
    }
    return fail(contents.error());
}

inline result<std::chrono::system_clock::time_point> boot_time() {
    const result<std::chrono::milliseconds> elapsed = uptime();
    if (!elapsed) {
        return fail(elapsed.error());
    }
    struct timespec realtime {};
    if (::clock_gettime(CLOCK_REALTIME, &realtime) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const auto real_ms = static_cast<std::int64_t>(realtime.tv_sec) * 1000 +
                         static_cast<std::int64_t>(realtime.tv_nsec) / 1000000;
    const auto boot_ms = real_ms - elapsed->count();
    return std::chrono::system_clock::time_point(
        std::chrono::milliseconds(boot_ms));
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
