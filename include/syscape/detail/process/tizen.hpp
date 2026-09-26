#ifndef SYSCAPE_DETAIL_PROCESS_TIZEN_HPP
#define SYSCAPE_DETAIL_PROCESS_TIZEN_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
#if defined(SYSCAPE_TIZEN_PID)
    return static_cast<std::uint32_t>(SYSCAPE_TIZEN_PID);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> parent_process_id() {
#if defined(SYSCAPE_TIZEN_PPID)
    return static_cast<std::uint32_t>(SYSCAPE_TIZEN_PPID);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}

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
#if defined(SYSCAPE_TIZEN_THREAD_COUNT)
    return static_cast<std::uint32_t>(SYSCAPE_TIZEN_THREAD_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

inline result<int> priority() {
#if defined(SYSCAPE_TIZEN_TASK_PRIORITY)
    return static_cast<int>(SYSCAPE_TIZEN_TASK_PRIORITY);
#elif defined(SYSCAPE_TIZEN_PRIORITY)
    return static_cast<int>(SYSCAPE_TIZEN_PRIORITY);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource /*resource*/) {
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
