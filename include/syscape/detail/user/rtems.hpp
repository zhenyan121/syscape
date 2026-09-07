#ifndef SYSCAPE_DETAIL_USER_RTEMS_HPP
#define SYSCAPE_DETAIL_USER_RTEMS_HPP

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <unistd.h>
#include <vector>

#include <syscape/detail/user/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

template <typename NativeIdentifier>
inline result<std::uint32_t>
narrow_rtems_identifier(NativeIdentifier value) noexcept {
    static_assert(std::is_integral<NativeIdentifier>::value,
                  "An RTEMS identifier must be an integral type");
    static_assert(std::is_unsigned<NativeIdentifier>::value,
                  "An RTEMS identifier must be an unsigned type");
    const std::uintmax_t widened = static_cast<std::uintmax_t>(value);
    if (widened > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(widened);
}

inline result<std::uint32_t> real_user_id() {
    return narrow_rtems_identifier(::getuid());
}

inline result<std::uint32_t> effective_user_id() {
    return narrow_rtems_identifier(::geteuid());
}

inline result<std::uint32_t> real_group_id() {
    return narrow_rtems_identifier(::getgid());
}

inline result<std::uint32_t> effective_group_id() {
    return narrow_rtems_identifier(::getegid());
}

inline result<std::string> user_name() {
    return fail(errc::not_supported);
}

inline result<std::string> home_directory() {
    return fail(errc::not_supported);
}

inline result<std::string> shell() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> supplementary_groups() {
    return fail(errc::not_supported);
}

inline result<user_common::privilege_state> privilege() {
    return ::geteuid() == 0 ? user_common::privilege_state::privileged
                            : user_common::privilege_state::unprivileged;
}

inline result<std::string> login_name() {
    return fail(errc::not_supported);
}

inline result<std::vector<user_common::session_info>> sessions() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
