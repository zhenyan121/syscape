#ifndef SYSCAPE_DETAIL_MEMORY_OPENBSD_HPP
#define SYSCAPE_DETAIL_MEMORY_OPENBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <uvm/uvm_extern.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<struct uvmexp> get_uvmexp() {
    int mib[] = {CTL_VM, VM_UVMEXP};
    struct uvmexp uvm {};
    std::size_t size = sizeof(uvm);
    if (::sysctl(mib, 2U, &uvm, &size, nullptr, 0U) != 0) {
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(uvm)) {
        return fail(errc::malformed_data);
    }
    return uvm;
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno != 0
                   ? result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::uint64_t>(fail(errc::malformed_data));
    }
    return static_cast<std::uint64_t>(value);
}

inline result<std::uint64_t> physical_memory_bytes() {
    int mib[] = {CTL_HW, HW_PHYSMEM64};
    std::uint64_t physmem = 0U;
    std::size_t size = sizeof(physmem);
    if (::sysctl(mib, 2U, &physmem, &size, nullptr, 0U) == 0) {
        if (size == sizeof(physmem) && physmem > 0U) {
            return physmem;
        }
        return fail(errc::malformed_data);
    }
    if (errno != ENOENT) {
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    int mib32[] = {CTL_HW, HW_PHYSMEM};
    int phys32 = 0;
    size = sizeof(phys32);
    if (::sysctl(mib32, 2U, &phys32, &size, nullptr, 0U) == 0) {
        if (size == sizeof(phys32) && phys32 > 0) {
            return static_cast<std::uint64_t>(phys32);
        }
        return fail(errc::malformed_data);
    }
    if (errno != ENOENT) {
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    auto uvm = get_uvmexp();
    if (!uvm) {
        return fail(uvm.error());
    }
    if (uvm->pagesize <= 0 || uvm->npages <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(uvm->npages) *
           static_cast<std::uint64_t>(uvm->pagesize);
}

inline result<std::uint64_t> available_memory_bytes() {
    auto uvm = get_uvmexp();
    if (!uvm) {
        return fail(uvm.error());
    }
    if (uvm->pagesize <= 0 || uvm->free < 0 || uvm->inactive < 0) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t free_pages = static_cast<std::uint64_t>(uvm->free);
    const std::uint64_t inactive_pages =
        static_cast<std::uint64_t>(uvm->inactive);
    const std::uint64_t page_size = static_cast<std::uint64_t>(uvm->pagesize);

    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (free_pages > max_u64 - inactive_pages) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t avail_pages = free_pages + inactive_pages;
    if (avail_pages > max_u64 / page_size) {
        return fail(errc::value_too_large);
    }
    return avail_pages * page_size;
}

inline result<memory_common::swap_usage> swap_status() {
    auto uvm = get_uvmexp();
    if (!uvm) {
        return fail(uvm.error());
    }
    if (uvm->pagesize <= 0 || uvm->swpages < 0 || uvm->swpginuse < 0 ||
        uvm->swpginuse > uvm->swpages) {
        return fail(errc::malformed_data);
    }

    const std::uint64_t page_size = static_cast<std::uint64_t>(uvm->pagesize);
    const std::uint64_t total_pages = static_cast<std::uint64_t>(uvm->swpages);
    const std::uint64_t used_pages = static_cast<std::uint64_t>(uvm->swpginuse);
    const std::uint64_t free_pages = total_pages - used_pages;

    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (total_pages > max_u64 / page_size || free_pages > max_u64 / page_size) {
        return fail(errc::value_too_large);
    }

    memory_common::swap_usage swap;
    swap.total_bytes = total_pages * page_size;
    swap.free_bytes = free_pages * page_size;
    return swap;
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

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

struct uint128_small {
    std::uint64_t hi = 0;
    std::uint64_t lo = 0;
};

inline uint128_small mul64_by_small(std::uint64_t val,
                                    std::uint32_t mult) noexcept {
    const std::uint64_t lo32 = val & 0xFFFFFFFFULL;
    const std::uint64_t hi32 = val >> 32;
    const std::uint64_t prod_lo = lo32 * static_cast<std::uint64_t>(mult);
    const std::uint64_t prod_hi =
        hi32 * static_cast<std::uint64_t>(mult) + (prod_lo >> 32);
    uint128_small result;
    result.lo = (prod_lo & 0xFFFFFFFFULL) | ((prod_hi & 0xFFFFFFFFULL) << 32);
    result.hi = prod_hi >> 32;
    return result;
}

inline bool uint128_lte(const uint128_small& a,
                        const uint128_small& b) noexcept {
    if (a.hi != b.hi) {
        return a.hi < b.hi;
    }
    return a.lo <= b.lo;
}

inline std::uint32_t exact_percent_u64(std::uint64_t used,
                                       std::uint64_t total) noexcept {
    constexpr std::uint64_t max_safe =
        (std::numeric_limits<std::uint64_t>::max)() / 100U;
    if (used <= max_safe) {
        return static_cast<std::uint32_t>((used * 100U) / total);
    }
    const uint128_small target = mul64_by_small(used, 100U);
    std::uint32_t low = 0U;
    std::uint32_t high = 100U;
    std::uint32_t ans = 0U;
    while (low <= high) {
        const std::uint32_t mid = low + (high - low) / 2U;
        const uint128_small mid_val = mul64_by_small(total, mid);
        if (uint128_lte(mid_val, target)) {
            ans = mid;
            low = mid + 1U;
        } else {
            if (mid == 0U) {
                break;
            }
            high = mid - 1U;
        }
    }
    return ans;
}

inline result<std::uint32_t> memory_load_percent() {
    const result<std::uint64_t> total = physical_memory_bytes();
    if (!total) {
        return fail(total.error());
    }
    const result<std::uint64_t> available = available_memory_bytes();
    if (!available) {
        return fail(available.error());
    }
    if (*total == 0U || *available > *total) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t used = *total - *available;
    return exact_percent_u64(used, *total);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
