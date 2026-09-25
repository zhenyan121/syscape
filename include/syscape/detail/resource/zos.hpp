#ifndef SYSCAPE_DETAIL_RESOURCE_ZOS_HPP
#define SYSCAPE_DETAIL_RESOURCE_ZOS_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>

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
#if defined(SYSCAPE_ZOS_PROCESS_COUNT)
    return static_cast<std::uint64_t>(SYSCAPE_ZOS_PROCESS_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_ZOS_SYSTEM_THREAD_COUNT)
    return static_cast<std::uint64_t>(SYSCAPE_ZOS_SYSTEM_THREAD_COUNT);
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
#if defined(SYSCAPE_ZOS_MAX_FILES)
    return static_cast<std::uint64_t>(SYSCAPE_ZOS_MAX_FILES);
#else
    return fail(errc::not_supported);
#endif
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
