#ifndef SYSCAPE_DETAIL_PROCESS_LIST_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_APPLE_MOBILE_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<std::vector<process_list::process_entry>> processes() {
    return fail(errc::permission_denied);
}

inline result<std::uint32_t> process_count() {
    return fail(errc::permission_denied);
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    return fail(errc::permission_denied);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view /*name*/) {
    return fail(errc::permission_denied);
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
