#ifndef SYSCAPE_DETAIL_ENVIRONMENT_GENERIC_HPP
#define SYSCAPE_DETAIL_ENVIRONMENT_GENERIC_HPP

#include <string>
#include <string_view>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace environment_backend {

inline result<std::string> get(std::string_view /*name*/) {
    return fail(errc::not_supported);
}

inline result<bool> has(std::string_view /*name*/) {
    return fail(errc::not_supported);
}

inline result<std::string> temp_directory() {
    return fail(errc::not_supported);
}

inline result<std::string> home_directory() {
    return fail(errc::not_supported);
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

inline result<bool> is_interactive_stdin() {
    return fail(errc::not_supported);
}

inline result<bool> is_interactive_stdout() {
    return fail(errc::not_supported);
}

inline result<bool> is_interactive_stderr() {
    return fail(errc::not_supported);
}

} // namespace environment_backend
} // namespace detail
} // namespace syscape

#endif
