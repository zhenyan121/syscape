#ifndef SYSCAPE_DETAIL_ENVIRONMENT_NUTTX_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_NUTTX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <sys/stat.h>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/environment/posix_common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_backend {

inline result<std::string> get(std::string_view name) {
#if defined(__NuttX__) && defined(CONFIG_DISABLE_ENVIRON)
    (void)name;
    return fail(errc::not_supported);
#else
    return environment_posix::get(name);
#endif
}

inline result<bool> has(std::string_view name) {
#if defined(__NuttX__) && defined(CONFIG_DISABLE_ENVIRON)
    (void)name;
    return fail(errc::not_supported);
#else
    return environment_posix::has(name);
#endif
}

inline result<std::vector<::syscape::environment::environment_variable>>
environment_variables() {
#if defined(__NuttX__) && defined(CONFIG_DISABLE_ENVIRON)
    return fail(errc::not_supported);
#else
    return environment_posix::environment_variables();
#endif
}

inline result<std::string> current_working_directory() {
    std::size_t size = 128U;
    constexpr std::size_t maximum = 1024U * 1024U;
    while (size <= maximum) {
        std::vector<char> buffer(size);
        errno = 0;
        if (::getcwd(buffer.data(), buffer.size()) != nullptr) {
            std::string path(buffer.data());
            if (!is_valid_utf8(path)) {
                return fail(errc::invalid_encoding);
            }
            return environment_common::normalize_directory_path(
                std::move(path));
        }
        const int error = errno;
        if (error == ERANGE) {
            size *= 2U;
            continue;
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> nuttx_absolute_normalized_path(std::string path) {
    if (!path.empty() && path.front() != '/') {
        const auto directory = current_working_directory();
        if (!directory) {
            return fail(directory.error());
        }
        path = *directory + (directory->back() == '/' ? "" : "/") + path;
    }
    return environment_common::validate_utf8_path(
        environment_posix::make_absolute_normalized_path(std::move(path)));
}

inline result<std::string> find_executable(std::string_view name) {
    if (name.empty() || name.find('\0') != std::string_view::npos) {
        return fail(errc::invalid_argument);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    const auto is_executable_file = [](const std::string& path) {
        struct ::stat status {};
        return ::stat(path.c_str(), &status) == 0 && S_ISREG(status.st_mode) &&
               ::access(path.c_str(), X_OK) == 0;
    };
    if (name.find('/') != std::string_view::npos) {
        const std::string path(name);
        return is_executable_file(path) ? nuttx_absolute_normalized_path(path)
                                        : fail(errc::not_found);
    }
#if defined(__NuttX__) && defined(CONFIG_DISABLE_ENVIRON)
    return fail(errc::not_supported);
#else
    const char* path_value = ::getenv("PATH");
    if (path_value == nullptr) {
        return fail(errc::not_found);
    }
    std::string_view remaining(path_value);
    for (;;) {
        const std::size_t separator = remaining.find(':');
        const std::string_view directory =
            separator == std::string_view::npos
                ? remaining
                : remaining.substr(0U, separator);
        std::string candidate =
            directory.empty() ? std::string(name) : std::string(directory);
        if (!directory.empty() && candidate.back() != '/') {
            candidate.push_back('/');
        }
        if (!directory.empty()) {
            candidate.append(name);
        }
        if (is_executable_file(candidate)) {
            if (!is_valid_utf8(candidate)) {
                return fail(errc::invalid_encoding);
            }
            return nuttx_absolute_normalized_path(std::move(candidate));
        }
        if (separator == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(separator + 1U);
    }
    return fail(errc::not_found);
#endif
}

inline result<bool> is_interactive_stdin() {
    return environment_posix::is_interactive_stdin();
}
inline result<bool> is_interactive_stdout() {
    return environment_posix::is_interactive_stdout();
}
inline result<bool> is_interactive_stderr() {
    return environment_posix::is_interactive_stderr();
}

inline result<std::string>
nuttx_existing_environment_directory(const char* name) {
#if defined(__NuttX__) && defined(CONFIG_DISABLE_ENVIRON)
    (void)name;
    return fail(errc::not_supported);
#else
    const char* value = ::getenv(name);
    if (value == nullptr || value[0] != '/') {
        return fail(errc::not_found);
    }
    struct ::stat status {};
    errno = 0;
    if (::stat(value, &status) != 0) {
        const int error = errno;
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(error, std::generic_category()));
    }
    if (!S_ISDIR(status.st_mode)) {
        return fail(errc::not_found);
    }
    return environment_common::normalize_directory_path(std::string(value));
#endif
}

inline result<std::string> temp_directory() {
#if !defined(__NuttX__) || !defined(CONFIG_DISABLE_ENVIRON)
    if (::getenv("TMPDIR") != nullptr) {
        return nuttx_existing_environment_directory("TMPDIR");
    }
#endif
    struct ::stat status {};
    errno = 0;
    if (::stat("/tmp", &status) == 0) {
        return S_ISDIR(status.st_mode) ? result<std::string>("/tmp")
                                       : fail(errc::not_found);
    }
    const int error = errno;
    if (error == ENOENT || error == ENOTDIR) {
        return fail(errc::not_found);
    }
    if (error == EACCES || error == EPERM) {
        return fail(errc::permission_denied);
    }
    return fail(std::error_code(error, std::generic_category()));
}

inline result<std::string> home_directory() {
    return nuttx_existing_environment_directory("HOME");
}
inline result<std::string> config_directory() {
    return fail(errc::not_supported);
}
inline result<std::string> data_directory() {
    return fail(errc::not_supported);
}
inline result<std::string> cache_directory() {
    return fail(errc::not_supported);
}

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
