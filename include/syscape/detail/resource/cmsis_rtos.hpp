#ifndef SYSCAPE_DETAIL_RESOURCE_CMSIS_RTOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_CMSIS_RTOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

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

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    return fail(errc::not_supported);
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_CMSIS_RTOS_THREAD_COUNT)
    return static_cast<std::uint64_t>(SYSCAPE_CMSIS_RTOS_THREAD_COUNT);
#elif defined(CMSIS_RTOS_THREAD_COUNT)
    return static_cast<std::uint64_t>(CMSIS_RTOS_THREAD_COUNT);
#elif defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#if defined(osCMSIS) && (osCMSIS >= 0x20000U)
    const std::uint32_t count = osThreadGetCount();
    return static_cast<std::uint64_t>(count);
#else
    return fail(errc::not_supported);
#endif
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_CMSIS_RTOS_HAS_KERNEL_HEADERS
#endif

#endif // SYSCAPE_DETAIL_RESOURCE_CMSIS_RTOS_HPP
