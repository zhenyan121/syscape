#ifndef SYSCAPE_DETAIL_ENVIRONMENT_POSIX_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_POSIX_HPP

#include <cstdlib>
#include <string>

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
    return std::string("/tmp");
}

inline result<std::string> config_directory() {
    const char* xdg_config = ::getenv("XDG_CONFIG_HOME");
    if (xdg_config != nullptr && xdg_config[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg_config));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.config";
}

inline result<std::string> data_directory() {
    const char* xdg_data = ::getenv("XDG_DATA_HOME");
    if (xdg_data != nullptr && xdg_data[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg_data));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.local/share";
}

inline result<std::string> cache_directory() {
    const char* xdg_cache = ::getenv("XDG_CACHE_HOME");
    if (xdg_cache != nullptr && xdg_cache[0] == '/') {
        return environment_common::normalize_directory_path(std::string(xdg_cache));
    }
    const result<std::string> home = home_directory();
    if (!home) {
        return fail(home.error());
    }
    return *home + "/.cache";
}

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
