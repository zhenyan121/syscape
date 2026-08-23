#ifndef SYSCAPE_DETAIL_ENVIRONMENT_COMMON_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_COMMON_HPP

#include <string>
#include <string_view>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_common {

/// Validates an environment variable name before query.
///
/// An environment variable name must be non-empty, must not contain the '='
/// character or null characters, and must be valid UTF-8.
inline result<void> validate_variable_name(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    if (name.find('=') != std::string_view::npos ||
        name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    return {};
}

/// Validates a UTF-8 directory path string returned by a platform backend.
///
/// A valid directory path must be non-empty and well-formed UTF-8.
inline result<std::string> validate_utf8_path(result<std::string> value) {
    if (!value) {
        return fail(value.error());
    }
    if (value->empty()) {
        return fail(errc::malformed_data);
    }
    if (!is_valid_utf8(*value)) {
        return fail(errc::invalid_encoding);
    }
    return value;
}

/// Normalizes a path by removing trailing slashes/backslashes unless it is
/// the root directory itself.
inline std::string normalize_directory_path(std::string path) {
    if (path.empty()) {
        return path;
    }
#if defined(_WIN32)
    // On Windows, handle 'C:\' or '\' or '/' roots
    while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) {
        if (path.size() == 3 && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
            break; // Retain C:\ or C:/
        }
        path.pop_back();
    }
#else
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
#endif
    return path;
}

} // namespace environment_common
} // namespace detail
} // namespace syscape

#endif
