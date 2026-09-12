#ifndef SYSCAPE_DETAIL_MEMORY_TKERNEL_HPP
#define SYSCAPE_DETAIL_MEMORY_TKERNEL_HPP

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

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(TK_TOTAL_HEAP_SIZE)
    return static_cast<std::uint64_t>(TK_TOTAL_HEAP_SIZE);
#elif defined(TK_HEAP_SIZE)
    return static_cast<std::uint64_t>(TK_HEAP_SIZE);
#elif defined(UTK_TOTAL_HEAP_SIZE)
    return static_cast<std::uint64_t>(UTK_TOTAL_HEAP_SIZE);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
    T_RMPL mpl {};
#if defined(MPL_SELF)
    const ID mplid = MPL_SELF;
#else
    const ID mplid = 1;
#endif
    if (::tk_ref_mpl(mplid, &mpl) == 0) {
        return static_cast<std::uint64_t>(mpl.frsz);
    }
    return fail(errc::temporarily_unavailable);
#else
    return fail(errc::not_supported);
#endif
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
    const auto phys = physical_memory_bytes();
    if (!phys) {
        return fail(phys.error());
    }
    const auto avail = available_memory_bytes();
    if (!avail) {
        return fail(avail.error());
    }
    if (*avail > *phys) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*phys - *avail, *phys);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS)
#undef SYSCAPE_TKERNEL_HAS_KERNEL_HEADERS
#endif

#endif
