#ifndef SYSCAPE_DETAIL_USER_COMMON_HPP
#define SYSCAPE_DETAIL_USER_COMMON_HPP

#include <string>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_common {

/// Validates a user name reported by a platform backend.
///
/// A name must be non-empty and valid UTF-8. An empty name is malformed
/// platform data rather than valid information.
inline result<std::string> validate_utf8_name(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Validates a home-directory path reported by a platform backend.
///
/// The path must be non-empty and valid UTF-8. Platform backends enforce
/// their own absoluteness rules before the boundary validation.
inline result<std::string> validate_utf8_path(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Validates a login-shell value reported by a platform backend.
///
/// An empty shell is valid data where the platform records no shell, so only
/// the UTF-8 encoding is enforced.
inline result<std::string> validate_utf8_shell(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

} // namespace user_common
} // namespace detail
} // namespace syscape

#endif
