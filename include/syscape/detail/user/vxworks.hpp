#ifndef SYSCAPE_DETAIL_USER_VXWORKS_HPP
#define SYSCAPE_DETAIL_USER_VXWORKS_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/user/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

#if defined(SYSCAPE_TARGET_VXWORKS_RTP)

inline result<std::uint32_t> real_user_id() {
    return static_cast<std::uint32_t>(::getuid());
}

inline result<std::uint32_t> effective_user_id() {
    return static_cast<std::uint32_t>(::geteuid());
}

inline result<std::uint32_t> real_group_id() {
    return static_cast<std::uint32_t>(::getgid());
}

inline result<std::uint32_t> effective_group_id() {
    return static_cast<std::uint32_t>(::getegid());
}

inline result<user_common::privilege_state> privilege() {
    return ::geteuid() == 0 ? user_common::privilege_state::privileged
                            : user_common::privilege_state::unprivileged;
}

inline result<std::string> user_name() {
    // VxWorks does not provide a platform user account database.
    return fail(errc::not_supported);
}

inline result<std::string> home_directory() {
    // VxWorks does not provide a platform user account database.
    return fail(errc::not_supported);
}

inline result<std::string> shell() {
    // VxWorks does not provide a platform user account database.
    return fail(errc::not_supported);
}

inline result<std::vector<std::uint32_t>> supplementary_groups() {
    // VxWorks does not have a Unix supplementary group concept.
    return fail(errc::not_supported);
}

#else // Kernel mode

inline result<std::uint32_t> real_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_user_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> real_group_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> effective_group_id() {
    return fail(errc::not_supported);
}

inline result<user_common::privilege_state> privilege() {
    return fail(errc::not_supported);
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

#endif

inline result<std::string> login_name() {
    // VxWorks does not provide getlogin_r or login sessions.
    return fail(errc::not_supported);
}

inline result<std::vector<user_common::session_info>> sessions() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
