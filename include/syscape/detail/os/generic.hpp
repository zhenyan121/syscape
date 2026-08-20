#ifndef SYSCAPE_DETAIL_OS_GENERIC_HPP
#define SYSCAPE_DETAIL_OS_GENERIC_HPP

#include <chrono>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace os_backend {

inline result<std::string> product_name() { return fail(errc::not_supported); }
inline result<std::string> product_version() { return fail(errc::not_supported); }
inline result<std::string> build_identifier() { return fail(errc::not_supported); }
inline result<std::string> kernel_name() { return fail(errc::not_supported); }
inline result<std::string> kernel_version() { return fail(errc::not_supported); }
inline result<std::string> host_name() { return fail(errc::not_supported); }
inline result<std::string> boot_identifier() { return fail(errc::not_supported); }
inline result<std::chrono::milliseconds> uptime() { return fail(errc::not_supported); }
inline result<std::chrono::system_clock::time_point> boot_time() {
    return fail(errc::not_supported);
}

} // namespace os_backend
} // namespace detail
} // namespace syscape

#endif
