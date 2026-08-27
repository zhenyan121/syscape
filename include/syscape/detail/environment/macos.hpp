#ifndef SYSCAPE_DETAIL_ENVIRONMENT_MACOS_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_MACOS_HPP

#include <cstdlib>
#include <cstddef>
#include <string>
#include <vector>

#include <unistd.h>

#include <syscape/detail/environment/common.hpp>
#include <syscape/detail/environment/posix_common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_backend {

using environment_posix::get;
using environment_posix::has;
using environment_posix::environment_variables;
using environment_posix::current_working_directory;
using environment_posix::find_executable;
using environment_posix::home_directory;
using environment_posix::is_interactive_stderr;
using environment_posix::is_interactive_stdin;
using environment_posix::is_interactive_stdout;

inline result<std::string> temp_directory() {
    const char* tmpdir = ::getenv("TMPDIR");
    if (tmpdir != nullptr && tmpdir[0] == '/') {
        return environment_common::normalize_directory_path(std::string(tmpdir));
    }

    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);
    for (std::size_t attempt = 0; attempt < 5U; ++attempt) {
        const std::size_t required = ::confstr(
            _CS_DARWIN_USER_TEMP_DIR, buffer.data(), buffer.size());
        if (required == 0U) {
            return std::string("/tmp");
        }
        if (required <= buffer.size()) {
            if (buffer[0] == '/') {
                return environment_common::normalize_directory_path(
                    std::string(buffer.data()));
            }
            return std::string("/tmp");
        }
        if (required > maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(required);
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::string> config_directory() {
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/Library/Application Support";
}

inline result<std::string> data_directory() {
    return config_directory();
}

inline result<std::string> cache_directory() {
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/Library/Caches";
}

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
