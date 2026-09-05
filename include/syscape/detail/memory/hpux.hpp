#ifndef SYSCAPE_DETAIL_MEMORY_HPUX_HPP
#define SYSCAPE_DETAIL_MEMORY_HPUX_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <system_error>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<sys/pstat.h>)
#include <sys/pstat.h>
#define SYSCAPE_HAS_HPUX_PSTAT 1
#endif
#endif

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long ps = ::sysconf(_SC_PAGESIZE);
    if (ps > 0) {
        return static_cast<std::uint64_t>(ps);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_static pst {};
    errno = 0;
    const int static_count = ::pstat_getstatic(&pst, sizeof(pst), 1, 0);
    if (static_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (static_count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (static_count != 1) {
        return fail(errc::malformed_data);
    }
    if (pst.physical_memory <= 0 || pst.page_size <= 0) {
        return fail(errc::malformed_data);
    }
    const auto mem = static_cast<std::uint64_t>(pst.physical_memory);
    const auto ps = static_cast<std::uint64_t>(pst.page_size);
    if (mem > UINT64_MAX / ps) {
        return fail(errc::value_too_large);
    }
    return mem * ps;
#endif
#if defined(_SC_PHYS_PAGES)
    errno = 0;
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    if (pages > 0) {
        const auto ps = page_size_bytes();
        if (!ps) {
            return fail(ps.error());
        }
        const auto p = static_cast<std::uint64_t>(pages);
        if (p > UINT64_MAX / *ps) {
            return fail(errc::value_too_large);
        }
        return p * (*ps);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_static pst {};
    struct pst_dynamic psd {};
    errno = 0;
    const int static_count = ::pstat_getstatic(&pst, sizeof(pst), 1, 0);
    if (static_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (static_count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (static_count != 1) {
        return fail(errc::malformed_data);
    }
    if (pst.page_size <= 0) {
        return fail(errc::malformed_data);
    }
    errno = 0;
    const int dynamic_count = ::pstat_getdynamic(&psd, sizeof(psd), 1, 0);
    if (dynamic_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (dynamic_count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (dynamic_count != 1 || psd.psd_free < 0) {
        return fail(errc::malformed_data);
    }
    const auto free_p = static_cast<std::uint64_t>(psd.psd_free);
    const auto ps = static_cast<std::uint64_t>(pst.page_size);
    if (free_p > UINT64_MAX / ps) {
        return fail(errc::value_too_large);
    }
    return free_p * ps;
#endif
#if defined(_SC_AVPHYS_PAGES)
    errno = 0;
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    if (pages > 0) {
        const auto ps = page_size_bytes();
        if (!ps) {
            return fail(ps.error());
        }
        const auto p = static_cast<std::uint64_t>(pages);
        if (p > UINT64_MAX / *ps) {
            return fail(errc::value_too_large);
        }
        return p * (*ps);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<memory_common::swap_usage> swap_status() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_static pst {};
    errno = 0;
    const int static_count = ::pstat_getstatic(&pst, sizeof(pst), 1, 0);
    if (static_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (static_count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (static_count != 1) {
        return fail(errc::malformed_data);
    }
    if (pst.page_size <= 0) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(pst.page_size);

    std::uint64_t total_bytes = 0;
    std::uint64_t free_bytes = 0;
    int index = 0;
    struct pst_swapinfo psw[16];
    int count = 0;
    while (true) {
        errno = 0;
        count = ::pstat_getswap(psw, sizeof(struct pst_swapinfo), 16, index);
        if (count < 0) {
            const int saved_errno = errno;
            if (saved_errno == EACCES || saved_errno == EPERM) {
                return fail(errc::permission_denied);
            }
            if (saved_errno != 0) {
                return fail(
                    std::error_code(saved_errno, std::generic_category()));
            }
            return fail(errc::io_error);
        }
        if (count == 0) {
            break;
        }
        if (count > 16) {
            return fail(errc::malformed_data);
        }
        for (int i = 0; i < count; ++i) {
#if defined(SW_ENABLED)
            if (!(psw[i].pss_flags & SW_ENABLED)) {
                continue;
            }
#endif
            std::uint64_t dev_total = 0;
#if defined(SW_FS)
            if (psw[i].pss_flags & SW_FS) {
                const auto limit = static_cast<std::uint64_t>(psw[i].pss_limit);
                const auto chunk =
                    static_cast<std::uint64_t>(psw[i].pss_swapchunk);
                if (limit > 0 && chunk > UINT64_MAX / limit) {
                    return fail(errc::value_too_large);
                }
                const std::uint64_t product1 = limit * chunk;
                if (product1 > UINT64_MAX / 1024ULL) {
                    return fail(errc::value_too_large);
                }
                dev_total = product1 * 1024ULL;
            } else {
                const auto blks =
                    static_cast<std::uint64_t>(psw[i].pss_nblksenabled);
                if (blks > UINT64_MAX / 1024ULL) {
                    return fail(errc::value_too_large);
                }
                dev_total = blks * 1024ULL;
            }
#else
            const auto blks =
                static_cast<std::uint64_t>(psw[i].pss_nblksenabled);
            if (blks > UINT64_MAX / 1024ULL) {
                return fail(errc::value_too_large);
            }
            dev_total = blks * 1024ULL;
#endif
            if (UINT64_MAX - total_bytes < dev_total) {
                return fail(errc::value_too_large);
            }
            total_bytes += dev_total;

            const auto nfpgs = static_cast<std::uint64_t>(psw[i].pss_nfpgs);
            if (nfpgs > UINT64_MAX / page_size) {
                return fail(errc::value_too_large);
            }
            const std::uint64_t dev_free = nfpgs * page_size;
            if (UINT64_MAX - free_bytes < dev_free) {
                return fail(errc::value_too_large);
            }
            free_bytes += dev_free;
        }
        const auto last_index = psw[count - 1].pss_idx;
        if (last_index < static_cast<std::uint64_t>(index)) {
            return fail(errc::malformed_data);
        }
        if (last_index >=
            static_cast<std::uint64_t>((std::numeric_limits<int>::max)())) {
            return fail(errc::value_too_large);
        }
        index = static_cast<int>(last_index) + 1;
    }

    memory_common::swap_usage usage {};
    usage.total_bytes = total_bytes;
    usage.free_bytes = free_bytes;
    return usage;
#else
    return fail(errc::not_supported);
#endif
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

#endif
