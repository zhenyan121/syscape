#ifndef SYSCAPE_DETAIL_RESOURCE_UCOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_UCOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<os.h>)
#include <os.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_ii.h>)
#include <ucos_ii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
#elif __has_include(<ucos_iii.h>)
#include <ucos_iii.h>
#define SYSCAPE_UCOS_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
#if defined(OS_CFG_TICK_RATE_HZ) || defined(UCOS_III) ||                       \
    (defined(OS_VERSION) && OS_VERSION >= 30000U)
    return static_cast<std::uint64_t>(::OSTaskQty);
#else
    return static_cast<std::uint64_t>(::OSTaskCtr);
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

#if defined(SYSCAPE_UCOS_HAS_KERNEL_HEADERS)
#undef SYSCAPE_UCOS_HAS_KERNEL_HEADERS
#endif

#endif
