#ifndef SYSCAPE_DETAIL_RESOURCE_SERENITY_HPP
#define SYSCAPE_DETAIL_RESOURCE_SERENITY_HPP

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
    const int limit = ::getdtablesize();
    if (limit > 0) {
        return static_cast<std::uint64_t>(limit);
    }
    const int saved_errno = errno;
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> handle_limit() {
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
