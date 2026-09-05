#ifndef SYSCAPE_DETAIL_MEMORY_SERENITY_HPP
#define SYSCAPE_DETAIL_MEMORY_SERENITY_HPP

#include <cstdint>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    const int page_size = ::getpagesize();
    if (page_size <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(page_size);
}

inline result<std::uint64_t> physical_memory_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
    return fail(errc::not_supported);
}

inline result<memory_common::swap_usage> swap_status() {
    return fail(errc::not_supported);
}

inline result<memory_common::commit_usage> commit_status() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> huge_page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> memory_load_percent() {
    return fail(errc::not_supported);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
