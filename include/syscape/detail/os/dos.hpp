#ifndef SYSCAPE_DETAIL_OS_DOS_HPP
#define SYSCAPE_DETAIL_OS_DOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() {
#if defined(__FREEDOS__)
    return std::string("FreeDOS");
#else
    return std::string("MS-DOS");
#endif
}

inline result<std::string> product_version() {
#if defined(__DJGPP__)
    return std::to_string(_osmajor) + "." + (_osminor < 10 ? "0" : "") +
           std::to_string(_osminor);
#else
    return std::string("7.10");
#endif
}

inline result<std::string> build_identifier() {
    return fail(errc::not_supported);
}

inline result<std::string> kernel_name() {
#if defined(__FREEDOS__)
    return std::string("FreeDOS Kernel");
#else
    return std::string("MS-DOS Kernel");
#endif
}

inline result<std::string> kernel_version() {
    return product_version();
}

inline result<std::string> host_name() {
    return fail(errc::not_supported);
}

inline result<std::chrono::milliseconds> uptime() {
    // In DOS/DJGPP, clock() measures execution ticks from process/system start
    const std::clock_t ticks = std::clock();
    if (ticks == static_cast<std::clock_t>(-1)) {
        return fail(errc::io_error);
    }
    const auto ms = static_cast<std::uint64_t>(ticks) * 1000U / CLOCKS_PER_SEC;
    return std::chrono::milliseconds(
        static_cast<std::chrono::milliseconds::rep>(ms));
}

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
