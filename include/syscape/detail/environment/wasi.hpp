#ifndef SYSCAPE_DETAIL_ENVIRONMENT_WASI_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_WASI_HPP

#include <atomic>
#include <cerrno>
#include <cstddef>
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
        if (error == ENOSYS || error == ENOTSUP) {
            return fail(errc::not_supported);
        }
        if (error != 0) {
            return fail(std::error_code(error, std::generic_category()));
        }
        break;
    }
    return fail(errc::not_supported);
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

    // Verify ability to create a temporary file in the directory
    // In WASI, access(W_OK) does not guarantee PATH_CREATE_FILE/PATH_OPEN
    // capabilities.
    std::string base = path;
    if (base.back() != '/') {
        base.push_back('/');
    }
    base.append(".syscape_probe_tmp_");

    static std::atomic<std::uint64_t> counter {0U};

    int fd = -1;
    std::string probe;
    for (std::size_t attempt = 0; attempt < 32U; ++attempt) {
        probe =
            base + std::to_string(
                       counter.fetch_add(1U, std::memory_order_relaxed) + 1U);
        errno = 0;
        fd = ::open(probe.c_str(), O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd != -1) {
            break;
        }
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

    if (fd == -1) {
        return fail(errc::permission_denied);
    }

    while (::close(fd) != 0) {
        if (errno != EINTR) {
            break;
        }
    }
    while (::unlink(probe.c_str()) != 0) {
        if (errno != EINTR) {
            break;
        }
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
