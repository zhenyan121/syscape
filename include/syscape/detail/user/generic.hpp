#ifndef SYSCAPE_DETAIL_USER_GENERIC_HPP
#define SYSCAPE_DETAIL_USER_GENERIC_HPP

#include <cstdint>
#include <string>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

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

inline result<std::string> user_name() {
    return fail(errc::not_supported);
}

inline result<std::string> home_directory() {
    return fail(errc::not_supported);
}

inline result<std::string> shell() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
