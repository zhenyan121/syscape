#ifndef SYSCAPE_DETAIL_PROCESS_MYNEWT_HPP
#define SYSCAPE_DETAIL_PROCESS_MYNEWT_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<os/os.h>)
#include <os/os.h>
#define SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS 1
#endif
#endif
#if defined(__cplusplus)
}
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

inline result<int> priority() {
#if defined(SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS)
    const auto* task = os_sched_get_current_task();
    if (task != nullptr) {
        return static_cast<int>(task->t_prio);
    }
    return fail(errc::not_supported);
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

#if defined(SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS)
#undef SYSCAPE_MYNEWT_HAS_KERNEL_HEADERS
#endif

#endif
