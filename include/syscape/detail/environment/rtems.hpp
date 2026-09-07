#ifndef SYSCAPE_DETAIL_ENVIRONMENT_RTEMS_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_RTEMS_HPP

#include <cerrno>
#include <cstdlib>
#include <string>
#include <system_error>

#include <sys/stat.h>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/environment/posix_common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_backend {

using environment_posix::current_working_directory;
using environment_posix::environment_variables;
using environment_posix::find_executable;
using environment_posix::get;
using environment_posix::has;
using environment_posix::is_interactive_stderr;
using environment_posix::is_interactive_stdin;
using environment_posix::is_interactive_stdout;

inline result<std::string>
existing_directory_from_environment(const char* name) {
    const char* const value = ::getenv(name);
    if (value == nullptr || value[0] != '/') {
        return fail(errc::not_found);
    }
    for (;;) {
        struct ::stat status {};
        if (::stat(value, &status) == 0) {
            if (!S_ISDIR(status.st_mode)) {
                return fail(errc::not_found);
            }
            return environment_common::normalize_directory_path(
                std::string(value));
        }
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
        return fail(std::error_code(error, std::generic_category()));
    }
}

inline result<std::string> temp_directory() {
    if (::getenv("TMPDIR") != nullptr) {
        return existing_directory_from_environment("TMPDIR");
    }
    for (;;) {
        struct ::stat status {};
        if (::stat("/tmp", &status) == 0) {
            return S_ISDIR(status.st_mode) ? result<std::string>("/tmp")
                                           : fail(errc::not_found);
        }
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
        return fail(std::error_code(error, std::generic_category()));
    }
}

inline result<std::string> home_directory() {
    return existing_directory_from_environment("HOME");
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
