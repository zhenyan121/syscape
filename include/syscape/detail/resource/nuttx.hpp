#ifndef SYSCAPE_DETAIL_RESOURCE_NUTTX_HPP
#define SYSCAPE_DETAIL_RESOURCE_NUTTX_HPP

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    // NuttX sysinfo currently repeats an instantaneous CPU busy ratio in all
    // three slots; it is not a 1/5/15-minute load average.
    return fail(errc::not_supported);
}
inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}
inline result<std::uint64_t> process_count() {
    return fail(errc::not_supported);
}
inline result<std::uint64_t> thread_count() {
    return fail(errc::not_supported);
}
inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}
inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    errno = 0;
    const long value = ::sysconf(_SC_OPEN_MAX);
    if (value > 0) {
        return static_cast<std::uint64_t>(value);
    }
    if (value == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error == ENOSYS || error == EINVAL || error == 0) {
        return fail(errc::not_supported);
    }
    return fail(std::error_code(error, std::generic_category()));
}

inline result<std::uint64_t> handle_limit() {
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
