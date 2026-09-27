#ifndef SYSCAPE_DETAIL_RESOURCE_RTTHREAD_HPP
#define SYSCAPE_DETAIL_RESOURCE_RTTHREAD_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if !defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
#define SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS 1
#endif

#if defined(__has_include)
#if __has_include(<rtthread.h>)
#include <rtthread.h>
#define SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS 1
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
#if defined(SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS)
    const auto count = rt_object_get_length(RT_Object_Class_Thread);
    if (count >= 0) {
        return static_cast<std::uint64_t>(count);
    }
    return fail(errc::not_supported);
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

#if defined(SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS)
#undef SYSCAPE_RTTHREAD_HAS_KERNEL_HEADERS
#undef SYSCAPE_RTTHREAD_INTERNAL_KERNEL_HEADERS
#endif

#endif
