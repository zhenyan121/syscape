#ifndef SYSCAPE_DETAIL_ENVIRONMENT_VXWORKS_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_VXWORKS_HPP

#include <syscape/detail/config.hpp>

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

#if !defined(_WRS_KERNEL)
extern "C" {
extern char** environ;
}
#endif

namespace syscape {
namespace environment {
struct environment_variable;
}

namespace detail {
namespace environment_backend {

inline result<std::string> get(std::string_view name) {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    (void)name;
    return fail(errc::not_supported);
#else
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    const std::string null_terminated_name(name);
    const char* value = ::getenv(null_terminated_name.c_str());
    if (value == nullptr) {
        return fail(errc::not_found);
    }
    std::string val(value);
    if (!is_valid_utf8(val)) {
        return fail(errc::invalid_encoding);
    }
    return val;
#endif
}

inline result<bool> has(std::string_view name) {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    (void)name;
    return fail(errc::not_supported);
#else
    const result<void> check = environment_common::validate_variable_name(name);
    if (!check) {
        return fail(check.error());
    }
    const std::string null_terminated_name(name);
    return ::getenv(null_terminated_name.c_str()) != nullptr;
#endif
}

inline result<std::vector<::syscape::environment::environment_variable>>
environment_variables() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    if (environ == nullptr) {
        return std::vector<::syscape::environment::environment_variable> {};
    }
    std::vector<::syscape::environment::environment_variable> vars;
    for (char** current = environ; *current != nullptr; ++current) {
        const char* entry = *current;
        const char* eq = std::strchr(entry, '=');
        if (eq == nullptr || eq == entry) {
            continue;
        }

        const std::string_view name(entry,
                                    static_cast<std::size_t>(eq - entry));
        const std::string_view value(eq + 1);

        if (!is_valid_utf8(name) || !is_valid_utf8(value)) {
            return fail(errc::invalid_encoding);
        }

        vars.push_back(::syscape::environment::environment_variable {
            std::string(name), std::string(value)});
    }

    std::sort(
        vars.begin(), vars.end(),
        [](const ::syscape::environment::environment_variable& a,
           const ::syscape::environment::environment_variable& b) noexcept {
            if (a.name != b.name) {
                return a.name < b.name;
            }
            return a.value < b.value;
        });

    return vars;
#endif
}

inline result<std::string> current_working_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    std::size_t size = 256U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::vector<char> buf(size);
        errno = 0;
        if (::getcwd(buf.data(), size) != nullptr) {
            if (buf[0] == '\0') {
                return fail(errc::malformed_data);
            }
            std::string cwd(buf.data());
            if (!is_valid_utf8(cwd)) {
                return fail(errc::invalid_encoding);
            }
            return environment_common::normalize_directory_path(std::move(cwd));
        }
        const int err = errno;
        if (err == ERANGE) {
            size *= 2U;
            continue;
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0) {
            return fail(std::error_code(err, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return fail(errc::value_too_large);
#endif
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

    std::string normalized;
    normalized.reserve(path.size());
    std::size_t i = 0U;
    while (i < path.size()) {
        if (path[i] == '/') {
            while (i + 1U < path.size() && path[i + 1U] == '/') {
                ++i;
            }
            if (i + 2U < path.size() && path[i + 1U] == '.' &&
                path[i + 2U] == '/') {
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
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    (void)name;
    return fail(errc::not_supported);
#else
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
        struct ::stat st {};
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
#endif
}

inline result<std::string> temp_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    const char* const vars[] = {"TMPDIR", "TEMP", "TMP"};
    for (const char* var : vars) {
        if (const char* val = ::getenv(var)) {
            if (val[0] == '/') {
                struct ::stat st {};
                if (::stat(val, &st) == 0 && S_ISDIR(st.st_mode)) {
                    std::string dir(val);
                    if (!is_valid_utf8(dir)) {
                        return fail(errc::invalid_encoding);
                    }
                    return environment_common::normalize_directory_path(
                        std::move(dir));
                }
            }
        }
    }
    struct ::stat st {};
    if (::stat("/tmp", &st) == 0 && S_ISDIR(st.st_mode)) {
        return std::string("/tmp");
    }
    return fail(errc::not_found);
#endif
}

inline result<std::string> home_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    const char* home = ::getenv("HOME");
    if (home != nullptr && home[0] == '/') {
        std::string path(home);
        if (!is_valid_utf8(path)) {
            return fail(errc::invalid_encoding);
        }
        return environment_common::normalize_directory_path(std::move(path));
    }
    return fail(errc::not_found);
#endif
}

inline result<std::string> config_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    const char* xdg_config = ::getenv("XDG_CONFIG_HOME");
    if (xdg_config != nullptr && xdg_config[0] == '/') {
        return environment_common::normalize_directory_path(
            std::string(xdg_config));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.config";
#endif
}

inline result<std::string> data_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    const char* xdg_data = ::getenv("XDG_DATA_HOME");
    if (xdg_data != nullptr && xdg_data[0] == '/') {
        return environment_common::normalize_directory_path(
            std::string(xdg_data));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.local/share";
#endif
}

inline result<std::string> cache_directory() {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    return fail(errc::not_supported);
#else
    const char* xdg_cache = ::getenv("XDG_CACHE_HOME");
    if (xdg_cache != nullptr && xdg_cache[0] == '/') {
        return environment_common::normalize_directory_path(
            std::string(xdg_cache));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.cache";
#endif
}

inline result<bool> is_interactive_fd(int fd) {
#if defined(SYSCAPE_TARGET_VXWORKS_KERNEL)
    (void)fd;
    return fail(errc::not_supported);
#else
    if (::isatty(fd) == 1) {
        return true;
    }
    const int error = errno;
    if (error == ENOTTY || error == EINVAL || error == EBADF ||
        error == ENXIO || error == EOPNOTSUPP) {
        return false;
    }
    return fail(std::error_code(error, std::generic_category()));
#endif
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

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
