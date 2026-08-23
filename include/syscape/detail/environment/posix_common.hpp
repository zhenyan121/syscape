#ifndef SYSCAPE_DETAIL_ENVIRONMENT_POSIX_COMMON_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_POSIX_COMMON_HPP

#include <cerrno>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>

#include <unistd.h>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/posix/passwd.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_posix {

/// Copies a process environment value before returning to the caller.
inline result<std::string> get_raw(std::string_view name) {
    const std::string null_terminated_name(name);
    const char* value = ::getenv(null_terminated_name.c_str());
    if (value == nullptr) {
        return fail(errc::not_found);
    }
    return std::string(value);
}

inline result<std::string> get(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    result<std::string> value = get_raw(name);
    if (!value) {
        return fail(value.error());
    }
    if (!is_valid_utf8(*value)) {
        return fail(errc::invalid_encoding);
    }
    return value;
}

inline result<bool> has(std::string_view name) {
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    const result<std::string> value = get_raw(name);
    if (value) {
        return true;
    }
    if (value.error() == errc::not_found) {
        return false;
    }
    return fail(value.error());
}

inline result<std::string> passwd_home_directory() {
    const result<posix_passwd::fields> fields = posix_passwd::current_entry();
    if (!fields) {
        return fail(fields.error());
    }
    if (fields->directory.empty() || fields->directory.front() != '/') {
        return fail(errc::not_found);
    }
    return fields->directory;
}

inline result<std::string> home_directory() {
    const char* home = ::getenv("HOME");
    if (home != nullptr && home[0] == '/') {
        return environment_common::normalize_directory_path(std::string(home));
    }
    const result<std::string> passwd_home = passwd_home_directory();
    if (!passwd_home) {
        return fail(passwd_home.error());
    }
    return environment_common::normalize_directory_path(*passwd_home);
}

inline result<bool> is_interactive_fd(int fd) {
    if (::isatty(fd) == 1) {
        return true;
    }
    const int error = errno;
    if (error == ENOTTY || error == EINVAL || error == EBADF ||
        error == ENXIO || error == EOPNOTSUPP) {
        return false;
    }
    return fail(std::error_code(error, std::generic_category()));
}

inline result<bool> is_interactive_stdin() {
    return is_interactive_fd(STDIN_FILENO);
}

inline result<bool> is_interactive_stdout() {
    return is_interactive_fd(STDOUT_FILENO);
}

inline result<bool> is_interactive_stderr() {
    return is_interactive_fd(STDERR_FILENO);
}

} // namespace environment_posix
} // namespace detail
} // namespace syscape

#endif
