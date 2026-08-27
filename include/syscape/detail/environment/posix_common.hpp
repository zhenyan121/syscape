#ifndef SYSCAPE_DETAIL_ENVIRONMENT_POSIX_COMMON_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_POSIX_COMMON_HPP

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__) && defined(__MACH__)
#include <crt_externs.h>
#else
extern "C" char** environ;
#endif

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/posix/passwd.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_posix {

#if defined(__APPLE__) && defined(__MACH__)
inline char** get_environ_ptr() noexcept {
    return *_NSGetEnviron();
}
#else
inline char** get_environ_ptr() noexcept {
    return environ;
}
#endif

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

inline result<std::vector<::syscape::environment::environment_variable>>
environment_variables() {
    char** env = get_environ_ptr();
    if (env == nullptr) {
        return std::vector<::syscape::environment::environment_variable>{};
    }

    std::vector<::syscape::environment::environment_variable> vars;
    for (char** current = env; *current != nullptr; ++current) {
        const char* entry = *current;
        const char* eq = std::strchr(entry, '=');
        if (eq == nullptr || eq == entry) {
            continue;
        }

        const std::string_view name(entry, static_cast<std::size_t>(eq - entry));
        const std::string_view value(eq + 1);

        if (!is_valid_utf8(name) || !is_valid_utf8(value)) {
            return fail(errc::invalid_encoding);
        }

        vars.push_back(::syscape::environment::environment_variable{
            std::string(name), std::string(value)});
    }

    std::sort(vars.begin(), vars.end(),
              [](const ::syscape::environment::environment_variable& a,
                 const ::syscape::environment::environment_variable& b) noexcept {
                  if (a.name != b.name) {
                      return a.name < b.name;
                  }
                  return a.value < b.value;
              });

    return vars;
}

inline result<std::string> current_working_directory() {
    char* cwd = ::getcwd(nullptr, 0);
    if (cwd == nullptr) {
        const int err = errno;
        return fail(std::error_code(err, std::generic_category()));
    }
    struct free_guard {
        char* ptr;
        ~free_guard() { std::free(ptr); }
    } guard{cwd};

    if (cwd[0] == '\0') {
        return fail(errc::malformed_data);
    }
    std::string path(cwd);
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    return environment_common::normalize_directory_path(std::move(path));
}

inline result<std::string> make_absolute_normalized_path(std::string path) {
    if (path.empty()) {
        return fail(errc::invalid_argument);
    }
    if (path.front() != '/') {
        const auto cwd = current_working_directory();
        if (!cwd) {
            return fail(cwd.error());
        }
        std::string abs_path;
        abs_path.reserve(cwd->size() + 1U + path.size());
        abs_path.append(*cwd);
        if (abs_path.back() != '/') {
            abs_path.push_back('/');
        }
        abs_path.append(path);
        path = std::move(abs_path);
    }

    // Collapse multiple slashes and /./ segments
    std::string normalized;
    normalized.reserve(path.size());
    std::size_t i = 0U;
    while (i < path.size()) {
        if (path[i] == '/') {
            while (i + 1U < path.size() && path[i + 1U] == '/') {
                ++i;
            }
            if (i + 2U < path.size() && path[i + 1U] == '.' && path[i + 2U] == '/') {
                i += 2U;
                continue;
            }
            if (i + 2U == path.size() && path[i + 1U] == '.') {
                i += 2U;
                break;
            }
        }
        normalized.push_back(path[i]);
        ++i;
    }

    if (normalized.empty()) {
        normalized.push_back('/');
    } else if (normalized.size() > 1U && normalized.back() == '/') {
        normalized.pop_back();
    }
    return normalized;
}

inline result<std::string> find_executable(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    if (name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }

    auto is_executable_file = [](const std::string& path) -> bool {
        struct ::stat st;
        if (::stat(path.c_str(), &st) != 0) {
            return false;
        }
        if (!S_ISREG(st.st_mode)) {
            return false;
        }
        return ::access(path.c_str(), X_OK) == 0;
    };

    if (name.find('/') != std::string_view::npos) {
        const std::string path(name);
        if (is_executable_file(path)) {
            return make_absolute_normalized_path(path);
        }
        return fail(errc::not_found);
    }

    const char* path_env = ::getenv("PATH");
    if (path_env == nullptr) {
        return fail(errc::not_found);
    }

    std::string_view path_view(path_env);
    bool done = false;
    while (!done) {
        const std::size_t colon_pos = path_view.find(':');
        const std::string_view dir = (colon_pos == std::string_view::npos)
                                         ? path_view
                                         : path_view.substr(0, colon_pos);

        std::string candidate;
        if (dir.empty()) {
            candidate = std::string(name);
        } else {
            candidate.reserve(dir.size() + 1U + name.size());
            candidate.append(dir);
            if (candidate.back() != '/') {
                candidate.push_back('/');
            }
            candidate.append(name);
        }

        if (is_executable_file(candidate)) {
            if (!is_valid_utf8(candidate)) {
                return fail(errc::invalid_encoding);
            }
            return make_absolute_normalized_path(std::move(candidate));
        }

        if (colon_pos == std::string_view::npos) {
            done = true;
        } else {
            path_view.remove_prefix(colon_pos + 1U);
        }
    }

    return fail(errc::not_found);
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
