#ifndef SYSCAPE_DETAIL_PROCESS_GENERIC_HPP
#define SYSCAPE_DETAIL_PROCESS_GENERIC_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() { return fail(errc::not_supported); }
inline result<std::uint32_t> parent_process_id() {
    return fail(errc::not_supported);
}
inline result<std::string> executable_path() { return fail(errc::not_supported); }
inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}
inline result<std::string> working_directory() {
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
