#ifndef SYSCAPE_DETAIL_ENVIRONMENT_FUCHSIA_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_FUCHSIA_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
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
    return environment_posix::get(name);
}

inline result<bool> has(std::string_view name) {
    return environment_posix::has(name);
}

inline result<std::vector<::syscape::environment::environment_variable>>
environment_variables() {
    return environment_posix::environment_variables();
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
            if (size > maximum / 2U) {
                return fail(errc::value_too_large);
            }
            size *= 2U;
            continue;
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        if (error == ENOSYS || error == ENOTSUP) {
            return fail(errc::not_supported);
        }
        if (error != 0) {
            return fail(std::error_code(error, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<std::string> find_executable(std::string_view /*name*/) {
    return fail(errc::not_supported);
}

inline result<std::string> validate_writable_directory(std::string path) {
    if (!is_valid_utf8(path)) {
        return fail(errc::invalid_encoding);
    }
    if (path.empty() || path.front() != '/') {
        return fail(errc::not_found);
    }
    for (;;) {
        struct stat st {};
        errno = 0;
        if (::stat(path.c_str(), &st) != 0) {
            const int error = errno;
            if (error == EINTR) {
                continue;
            }
            if (error == ENOENT || error == ENOTDIR) {
                return fail(errc::not_found);
            }
            if (error == EACCES || error == EPERM) {
                return fail(errc::permission_denied);
            }
            if (error != 0) {
                return fail(std::error_code(error, std::generic_category()));
            }
            return fail(errc::not_found);
        }
        if (!S_ISDIR(st.st_mode)) {
            return fail(errc::not_found);
        }
        break;
    }

#if !defined(W_OK) || !defined(X_OK)
    return fail(errc::not_supported);
#else
    for (;;) {
        errno = 0;
        if (::access(path.c_str(), W_OK | X_OK) != 0) {
            const int error = errno;
            if (error == EINTR) {
                continue;
            }
            if (error == EACCES || error == EPERM || error == EROFS) {
                return fail(errc::permission_denied);
            }
            if (error == ENOENT || error == ENOTDIR) {
                return fail(errc::not_found);
            }
            if (error == ENOSYS || error == ENOTSUP || error == 0) {
                return fail(errc::not_supported);
            }
            return fail(std::error_code(error, std::generic_category()));
        }
        break;
    }
#endif

    std::string base = path;
    if (base.back() != '/') {
        base.push_back('/');
    }
    base.append(".syscape_probe_tmp_");

    struct probe_cleanup {
        int fd = -1;
        std::string path;

        ~probe_cleanup() {
            if (fd != -1) {
                ::close(fd);
            }
            if (!path.empty()) {
                while (::unlink(path.c_str()) != 0) {
                    if (errno != EINTR) {
                        break;
                    }
                }
            }
        }
    } cleanup;

    const auto pid = static_cast<std::uintmax_t>(::getpid());
    const auto seed =
        static_cast<std::uintmax_t>(reinterpret_cast<std::uintptr_t>(&cleanup));

    for (std::size_t attempt = 0; attempt < 32U; ++attempt) {
        cleanup.path =
            base + std::to_string(pid) + "_" +
            std::to_string(seed ^ static_cast<std::uintmax_t>(attempt));
        errno = 0;
        cleanup.fd =
            ::open(cleanup.path.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (cleanup.fd != -1) {
            break;
        }
        cleanup.path.clear();
        const int error = errno;
        if (error == EINTR) {
            continue;
        }
        if (error == EEXIST) {
            continue;
        }
        if (error == EACCES || error == EPERM || error == EROFS) {
            return fail(errc::permission_denied);
        }
#if defined(ENOTCAPABLE)
        if (error == ENOTCAPABLE) {
            return fail(errc::permission_denied);
        }
#endif
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (error == ENOSYS || error == ENOTSUP || error == 0) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(error, std::generic_category()));
    }

    if (cleanup.fd == -1) {
        return fail(errc::temporarily_unavailable);
    }

    int close_err = 0;
    errno = 0;
    if (::close(cleanup.fd) != 0) {
        close_err = errno;
    }
    cleanup.fd = -1;

    int unlink_err = 0;
    while (::unlink(cleanup.path.c_str()) != 0) {
        const int error = errno;
        if (error == EINTR) {
            continue;
        }
        unlink_err = error;
        break;
    }
    cleanup.path.clear();

    if (close_err != 0) {
        if (close_err == EACCES || close_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(close_err, std::generic_category()));
    }

    if (unlink_err != 0) {
        if (unlink_err == EACCES || unlink_err == EPERM ||
            unlink_err == EROFS) {
            return fail(errc::permission_denied);
        }
        if (unlink_err == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(unlink_err, std::generic_category()));
    }

    return environment_common::normalize_directory_path(std::move(path));
}

inline result<std::string> temp_directory() {
    const char* tmpdir = ::getenv("TMPDIR");
    if (tmpdir != nullptr) {
        if (tmpdir[0] == '\0') {
            return fail(errc::not_found);
        }
        return validate_writable_directory(std::string(tmpdir));
    }

    const auto res = validate_writable_directory(std::string("/tmp"));
    if (!res) {
        if (res.error() == errc::not_found) {
            return fail(errc::not_supported);
        }
        return fail(res.error());
    }
    return res;
}

inline result<std::string> home_directory() {
    const char* home = ::getenv("HOME");
    if (home != nullptr) {
        if (home[0] == '\0') {
            return fail(errc::not_found);
        }
        if (!is_valid_utf8(home)) {
            return fail(errc::invalid_encoding);
        }
        if (home[0] != '/') {
            return fail(errc::not_found);
        }
        for (;;) {
            struct stat st {};
            errno = 0;
            if (::stat(home, &st) != 0) {
                const int error = errno;
                if (error == EINTR) {
                    continue;
                }
                if (error == ENOENT || error == ENOTDIR) {
                    return fail(errc::not_found);
                }
                if (error == EACCES || error == EPERM) {
                    return fail(errc::permission_denied);
                }
                if (error != 0) {
                    return fail(
                        std::error_code(error, std::generic_category()));
                }
                return fail(errc::not_found);
            }
            if (!S_ISDIR(st.st_mode)) {
                return fail(errc::not_found);
            }
            return environment_common::normalize_directory_path(
                std::string(home));
        }
    }
    return fail(errc::not_supported);
}

inline result<std::string> config_directory() {
    const char* xdg = ::getenv("XDG_CONFIG_HOME");
    if (xdg != nullptr && xdg[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg));
    }
    const auto home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.config";
}

inline result<std::string> data_directory() {
    const char* xdg = ::getenv("XDG_DATA_HOME");
    if (xdg != nullptr && xdg[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg));
    }
    const auto home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.local/share";
}

inline result<std::string> cache_directory() {
    const char* xdg = ::getenv("XDG_CACHE_HOME");
    if (xdg != nullptr && xdg[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg));
    }
    const auto home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.cache";
}

using environment_posix::is_interactive_stderr;
using environment_posix::is_interactive_stdin;
using environment_posix::is_interactive_stdout;

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
