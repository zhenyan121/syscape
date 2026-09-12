#ifndef SYSCAPE_DETAIL_RESOURCE_INTEGRITY_HPP
#define SYSCAPE_DETAIL_RESOURCE_INTEGRITY_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

#if defined(__cplusplus)
extern "C" {
#endif
#if defined(__has_include)
#if __has_include(<INTEGRITY.h>)
#include <INTEGRITY.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
#elif __has_include(<integrity.h>)
#include <integrity.h>
#define SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS 1
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
#if defined(INTEGRITY_TASK_COUNT)
    return static_cast<std::uint64_t>(INTEGRITY_TASK_COUNT);
#elif defined(INTEGRITY_ACTIVE_TASKS)
    return static_cast<std::uint64_t>(INTEGRITY_ACTIVE_TASKS);
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

#if defined(SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS)
#undef SYSCAPE_INTEGRITY_HAS_KERNEL_HEADERS
#endif

#endif
