#ifndef SYSCAPE_DETAIL_PROCESS_GENERIC_HPP
#define SYSCAPE_DETAIL_PROCESS_GENERIC_HPP

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/process/common.hpp>
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
inline result<process_common::cpu_time_usage> cpu_time() {
    return fail(errc::not_supported);
}
inline result<std::chrono::system_clock::time_point> start_time() {
    return fail(errc::not_supported);
}
inline result<process_common::memory_usage_snapshot> memory_usage() {
    return fail(errc::not_supported);
}
inline result<std::uint32_t> thread_count() {
    return fail(errc::not_supported);
}
inline result<int> priority() { return fail(errc::not_supported); }
inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}
inline result<process_common::resource_limit_snapshot> resource_limit(
    process_common::limit_resource) {
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
