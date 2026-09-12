#ifndef SYSCAPE_DETAIL_RESOURCE_TKERNEL_HPP
#define SYSCAPE_DETAIL_RESOURCE_TKERNEL_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<tk/tkernel.h>)
#include <tk/tkernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
#elif __has_include(<tkernel.h>)
#include <tkernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
#elif __has_include(<t-kernel.h>)
#include <t-kernel.h>
#define SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS 1
#endif
#endif
#if defined(__cplusplus)
}
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
#if defined(TK_TASK_COUNT)
    return static_cast<std::uint64_t>(TK_TASK_COUNT);
#elif defined(TK_ACTIVE_TASKS)
    return static_cast<std::uint64_t>(TK_ACTIVE_TASKS);
#elif defined(UTK_TASK_COUNT)
    return static_cast<std::uint64_t>(UTK_TASK_COUNT);
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

#if defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
#undef SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS
#endif

#endif
