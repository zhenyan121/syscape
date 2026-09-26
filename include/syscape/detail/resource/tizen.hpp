#ifndef SYSCAPE_DETAIL_RESOURCE_TIZEN_HPP
#define SYSCAPE_DETAIL_RESOURCE_TIZEN_HPP

#include <syscape/detail/config.hpp>

#include <cstdint>
#include <type_traits>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

template <typename T>
inline result<std::uint64_t> validate_positive_u64_count(T value) {
    if constexpr (std::is_signed<T>::value) {
        if (value <= 0) {
            return fail(errc::malformed_data);
        }
    } else {
        if (value == 0) {
            return fail(errc::malformed_data);
        }
    }
    return static_cast<std::uint64_t>(value);
}

/// System load averages are not exposed without /proc/loadavg access.
inline result<resource_common::load_samples> load_average() {
    return fail(errc::not_supported);
}

/// Schedulable entity counts are not exposed without /proc/loadavg access.
inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

/// Returns the system process count if supplied by override macro.
inline result<std::uint64_t> process_count() {
#if defined(SYSCAPE_TIZEN_PROCESS_COUNT)
    return validate_positive_u64_count(SYSCAPE_TIZEN_PROCESS_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Returns the system thread count if supplied by override macro.
inline result<std::uint64_t> thread_count() {
#if defined(SYSCAPE_TIZEN_SYSTEM_THREAD_COUNT)
    return validate_positive_u64_count(SYSCAPE_TIZEN_SYSTEM_THREAD_COUNT);
#else
    return fail(errc::not_supported);
#endif
}

/// Open file count across all processes is not accessible in unprivileged
/// sandboxes.
inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

/// Open handle count across all processes is not accessible in unprivileged
/// sandboxes.
inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

/// Returns the process file descriptor limit if supplied by override macro.
inline result<std::uint64_t> file_descriptor_limit() {
#if defined(SYSCAPE_TIZEN_MAX_FILES)
    return validate_positive_u64_count(SYSCAPE_TIZEN_MAX_FILES);
#else
    return fail(errc::not_supported);
#endif
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
