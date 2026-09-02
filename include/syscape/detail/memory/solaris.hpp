#ifndef SYSCAPE_DETAIL_MEMORY_SOLARIS_HPP
#define SYSCAPE_DETAIL_MEMORY_SOLARIS_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sys/stat.h>
#include <sys/swap.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline bool safe_add_u64(std::uint64_t a, std::uint64_t b,
                         std::uint64_t& out) noexcept {
    if (a > (std::numeric_limits<std::uint64_t>::max)() - b) {
        return false;
    }
    out = a + b;
    return true;
}

inline bool safe_multiply_u64(std::uint64_t a, std::uint64_t b,
                              std::uint64_t& out) {
    if (a == 0U || b == 0U) {
        out = 0U;
        return true;
    }
    if (a > UINT64_MAX / b) {
        return false;
    }
    out = a * b;
    return true;
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long size = ::sysconf(_SC_PAGESIZE);
    if (size < 0) {
        if (errno != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (size == 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(size);
}

inline result<std::uint64_t> physical_memory_bytes() {
    const auto page_size = page_size_bytes();
    if (!page_size) {
        return fail(page_size.error());
    }
    errno = 0;
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    if (pages < 0) {
        if (errno != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (pages == 0) {
        return fail(errc::malformed_data);
    }
    std::uint64_t result_val = 0U;
    if (!safe_multiply_u64(static_cast<std::uint64_t>(pages), *page_size,
                           result_val)) {
        return fail(errc::value_too_large);
    }
    return result_val;
}

inline result<std::uint64_t> available_memory_bytes() {
    const auto page_size = page_size_bytes();
    if (!page_size) {
        return fail(page_size.error());
    }
    errno = 0;
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    if (pages < 0) {
        if (errno != 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    std::uint64_t result_val = 0U;
    if (!safe_multiply_u64(static_cast<std::uint64_t>(pages), *page_size,
                           result_val)) {
        return fail(errc::value_too_large);
    }
    return result_val;
}

#if defined(SC_GETNSWP) && defined(SC_LIST)

struct swap_table_holder {
    struct ::swaptable* ptr = nullptr;
    char* path_storage = nullptr;
    std::size_t capacity = 0U;

    enum class alloc_status { success, overflow, out_of_memory };

    alloc_status allocate(std::size_t n) {
        if (n == 0U) {
            return alloc_status::overflow;
        }
        if (n > static_cast<std::size_t>((std::numeric_limits<int>::max)())) {
            return alloc_status::overflow;
        }
        if (n >
            (SIZE_MAX - sizeof(struct ::swaptable)) / sizeof(struct ::swapent) +
                1U) {
            return alloc_status::overflow;
        }
        if (n > SIZE_MAX / 1024U) {
            return alloc_status::overflow;
        }

        const std::size_t bytes =
            sizeof(struct ::swaptable) + (n - 1U) * sizeof(struct ::swapent);
        void* raw = std::malloc(bytes);
        if (raw == nullptr) {
            return alloc_status::out_of_memory;
        }

        void* paths_raw = std::malloc(n * 1024U);
        if (paths_raw == nullptr) {
            std::free(raw);
            return alloc_status::out_of_memory;
        }

        if (ptr != nullptr) {
            std::free(ptr);
        }
        if (path_storage != nullptr) {
            std::free(path_storage);
        }

        ptr = static_cast<struct ::swaptable*>(raw);
        path_storage = static_cast<char*>(paths_raw);
        std::memset(path_storage, 0, n * 1024U);

        ptr->swt_n = static_cast<int>(n);
        capacity = n;
        for (std::size_t i = 0U; i < n; ++i) {
            ptr->swt_ent[i].ste_path = path_storage + (i * 1024U);
        }
        return alloc_status::success;
    }

    ~swap_table_holder() {
        if (ptr != nullptr) {
            std::free(ptr);
        }
        if (path_storage != nullptr) {
            std::free(path_storage);
        }
    }

    swap_table_holder(const swap_table_holder&) = delete;
    swap_table_holder& operator=(const swap_table_holder&) = delete;
    swap_table_holder() = default;
};

#endif

inline result<memory_common::swap_usage> swap_status() {
#if defined(SC_GETNSWP) && defined(SC_LIST)
    constexpr int max_retries = 5;
    for (int attempt = 0; attempt < max_retries; ++attempt) {
        errno = 0;
        const int num_swaps = ::swapctl(SC_GETNSWP, nullptr);
        if (num_swaps < 0) {
            const int err = errno != 0 ? errno : EIO;
            return fail(std::error_code(err, std::generic_category()));
        }
        if (num_swaps == 0) {
            memory_common::swap_usage usage {};
            usage.total_bytes = 0U;
            usage.free_bytes = 0U;
            return usage;
        }

        const auto page_size = page_size_bytes();
        if (!page_size) {
            return fail(page_size.error());
        }

        if (static_cast<std::size_t>(num_swaps) >
            static_cast<std::size_t>((std::numeric_limits<int>::max)()) - 4U) {
            return fail(errc::value_too_large);
        }
        const std::size_t alloc_count =
            static_cast<std::size_t>(num_swaps) + 4U;
        swap_table_holder holder;
        const auto status = holder.allocate(alloc_count);
        if (status == swap_table_holder::alloc_status::overflow) {
            return fail(errc::value_too_large);
        }
        if (status == swap_table_holder::alloc_status::out_of_memory) {
            return fail(std::error_code(ENOMEM, std::generic_category()));
        }

        errno = 0;
        const int count = ::swapctl(SC_LIST, holder.ptr);
        if (count < 0) {
            if (errno == ENOMEM || errno == E2BIG) {
                continue;
            }
            const int err = errno != 0 ? errno : EIO;
            return fail(std::error_code(err, std::generic_category()));
        }

        if (static_cast<std::size_t>(count) > holder.capacity) {
            continue;
        }

        std::uint64_t total_pages = 0U;
        std::uint64_t free_pages = 0U;
        for (std::size_t i = 0U; i < static_cast<std::size_t>(count); ++i) {
            if (holder.ptr->swt_ent[i].ste_pages < 0 ||
                holder.ptr->swt_ent[i].ste_free < 0) {
                return fail(errc::malformed_data);
            }
            if (!safe_add_u64(total_pages,
                              static_cast<std::uint64_t>(
                                  holder.ptr->swt_ent[i].ste_pages),
                              total_pages) ||
                !safe_add_u64(
                    free_pages,
                    static_cast<std::uint64_t>(holder.ptr->swt_ent[i].ste_free),
                    free_pages)) {
                return fail(errc::value_too_large);
            }
        }

        if (free_pages > total_pages) {
            return fail(errc::malformed_data);
        }

        std::uint64_t total_b = 0U;
        std::uint64_t free_b = 0U;
        if (!safe_multiply_u64(total_pages, *page_size, total_b) ||
            !safe_multiply_u64(free_pages, *page_size, free_b)) {
            return fail(errc::value_too_large);
        }

        memory_common::swap_usage usage {};
        usage.total_bytes = total_b;
        usage.free_bytes = free_b;
        return usage;
    }
    return fail(errc::temporarily_unavailable);
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
