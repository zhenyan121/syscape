#ifndef SYSCAPE_DETAIL_PROCESS_RTTHREAD_HPP
#define SYSCAPE_DETAIL_PROCESS_RTTHREAD_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if !defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#define SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<rtthread.h>)
#include <rtthread.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> parent_process_id() {
    return fail(errc::not_supported);
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
    return fail(errc::not_supported);
}

// Returns the active thread's scheduling priority on RT-Thread.
// Note: In RT-Thread, 0 represents the highest priority and higher numeric
// values represent lower priority, which is the inverse of POSIX conventions.
inline result<int> priority() {
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
    const auto thread = rt_thread_self();
    if (thread != nullptr) {
        return static_cast<int>(thread->current_priority);
    }
    return fail(errc::not_found);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource resource) {
    (void)resource;
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
