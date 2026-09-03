#ifndef SYSCAPE_DETAIL_MEMORY_HAIKU_HPP
#define SYSCAPE_DETAIL_MEMORY_HAIKU_HPP

#include <cstdint>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
#if defined(B_PAGE_SIZE)
    return static_cast<std::uint64_t>(B_PAGE_SIZE);
#else
    const long ps = ::sysconf(_SC_PAGESIZE);
    if (ps > 0) {
        return static_cast<std::uint64_t>(ps);
    }
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.max_pages > 0) {
        return static_cast<std::uint64_t>(info.max_pages) *
               static_cast<std::uint64_t>(B_PAGE_SIZE);
    }
#endif
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const auto ps = page_size_bytes();
    if (pages > 0 && ps) {
        return static_cast<std::uint64_t>(pages) * (*ps);
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.max_pages >= info.used_pages) {
        return static_cast<std::uint64_t>(info.max_pages - info.used_pages) *
               static_cast<std::uint64_t>(B_PAGE_SIZE);
    }
#endif
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    const auto ps = page_size_bytes();
    if (pages > 0 && ps) {
        return static_cast<std::uint64_t>(pages) * (*ps);
    }
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
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info info {};
    if (::get_system_info(&info) == B_OK && info.max_pages > 0) {
        std::uint64_t used = static_cast<std::uint64_t>(info.used_pages);
        const std::uint64_t total = static_cast<std::uint64_t>(info.max_pages);
        if (used > total) {
            used = total;
        }
        return static_cast<std::uint32_t>((used * 100U) / total);
    }
#endif
    return fail(errc::not_supported);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
