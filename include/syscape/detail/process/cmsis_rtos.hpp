#ifndef SYSCAPE_DETAIL_PROCESS_CMSIS_RTOS_HPP
#define SYSCAPE_DETAIL_PROCESS_CMSIS_RTOS_HPP

#include <syscape/detail/config.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#if defined(__has_include)
#if defined(__CMSIS_RTOS) && !defined(__CMSIS_RTOS2) && __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os2.h>)
#include <cmsis_os2.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<cmsis_os.h>)
#include <cmsis_os.h>
#define SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS 1
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

// Returns the active thread's scheduling priority on CMSIS-RTOS / Keil RTX.
inline result<int> priority() {
#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
    const osThreadId_t id = osThreadGetId();
    if (id == nullptr) {
        return fail(errc::not_found);
    }
    const osPriority_t prio = osThreadGetPriority(id);
    if (static_cast<int>(prio) == static_cast<int>(osPriorityError)) {
        return fail(errc::not_found);
    }
    return static_cast<int>(prio);
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

#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS
#endif

#endif // SYSCAPE_DETAIL_PROCESS_CMSIS_RTOS_HPP
