#ifndef SYSCAPE_DETAIL_PROCESS_LIST_GENERIC_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_GENERIC_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<std::vector<process_list::process_entry>> processes() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> process_count() {
    return fail(errc::not_supported);
}

inline result<process_list::process_entry> find_process(std::uint32_t) {
    return fail(errc::not_supported);
}

inline result<std::vector<process_list::process_entry>> find_processes_by_name(
    std::string_view) {
    return fail(errc::not_supported);
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PROCESS_LIST_GENERIC_HPP
