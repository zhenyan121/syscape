#ifndef SYSCAPE_DETAIL_MEMORY_HURD_HPP
#define SYSCAPE_DETAIL_MEMORY_HURD_HPP

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string_view>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline bool is_memory_space(char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

inline std::string_view trim_memory(std::string_view s) noexcept {
    while (!s.empty() && is_memory_space(s.front())) {
        s.remove_prefix(1U);
    }
    while (!s.empty() && is_memory_space(s.back())) {
        s.remove_suffix(1U);
    }
    return s;
}

inline result<std::uint64_t> parse_meminfo_kb_line(const char* line,
                                                   const char* prefix) {
    const std::size_t prefix_len = std::strlen(prefix);
    if (std::strncmp(line, prefix, prefix_len) != 0) {
        return fail(errc::not_found);
    }
    std::string_view rest = trim_memory(line + prefix_len);
    if (rest.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint64_t kb = 0U;
    std::size_t idx = 0U;
    while (idx < rest.size() && rest[idx] >= '0' && rest[idx] <= '9') {
        const auto digit = static_cast<std::uint64_t>(rest[idx] - '0');
        if (kb > ((std::numeric_limits<std::uint64_t>::max)() - digit) / 10U) {
            return fail(errc::value_too_large);
        }
        kb = kb * 10U + digit;
        ++idx;
    }
    if (idx == 0U) {
        return fail(errc::malformed_data);
    }
    const std::string_view unit = trim_memory(rest.substr(idx));
    if (unit != "kB") {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t max_kb =
        (std::numeric_limits<std::uint64_t>::max)() >> 10U;
    if (kb > max_kb) {
        return fail(errc::value_too_large);
    }
    return kb << 10U;
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long ps = ::sysconf(_SC_PAGESIZE);
    if (ps > 0) {
        return static_cast<std::uint64_t>(ps);
    }
    const int saved_errno = errno;
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> physical_memory_bytes() {
    FILE* fp = std::fopen("/proc/meminfo", "r");
    if (fp != nullptr) {
        char line[256];
        bool found = false;
        std::uint64_t bytes = 0U;
        while (std::fgets(line, static_cast<int>(sizeof(line)), fp) !=
               nullptr) {
            auto parsed = parse_meminfo_kb_line(line, "MemTotal:");
            if (parsed) {
                bytes = *parsed;
                found = true;
                break;
            } else if (parsed.error() != make_error_code(errc::not_found)) {
                std::fclose(fp);
                return fail(parsed.error());
            }
        }
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (found) {
            return bytes;
        }
    } else {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != ENOENT) {
            return fail(std::error_code(err, std::generic_category()));
        }
    }

#if defined(_SC_PHYS_PAGES)
    errno = 0;
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    if (pages > 0) {
        const auto ps = page_size_bytes();
        if (!ps) {
            return fail(ps.error());
        }
        const auto u_pages = static_cast<std::uint64_t>(pages);
        if (u_pages > (std::numeric_limits<std::uint64_t>::max)() / (*ps)) {
            return fail(errc::value_too_large);
        }
        return u_pages * (*ps);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0 && saved_errno != EINVAL) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<std::uint64_t> available_memory_bytes() {
    FILE* fp = std::fopen("/proc/meminfo", "r");
    if (fp != nullptr) {
        char line[256];
        std::uint64_t mem_avail = 0U;
        std::uint64_t mem_free = 0U;
        bool found_avail = false;
        bool found_free = false;
        while (std::fgets(line, static_cast<int>(sizeof(line)), fp) !=
               nullptr) {
            if (!found_avail) {
                auto parsed = parse_meminfo_kb_line(line, "MemAvailable:");
                if (parsed) {
                    mem_avail = *parsed;
                    found_avail = true;
                    break;
                } else if (parsed.error() != make_error_code(errc::not_found)) {
                    std::fclose(fp);
                    return fail(parsed.error());
                }
            }
            if (!found_free) {
                auto parsed = parse_meminfo_kb_line(line, "MemFree:");
                if (parsed) {
                    mem_free = *parsed;
                    found_free = true;
                } else if (parsed.error() != make_error_code(errc::not_found)) {
                    std::fclose(fp);
                    return fail(parsed.error());
                }
            }
        }
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (found_avail) {
            return mem_avail;
        }
        if (found_free) {
            return mem_free;
        }
    } else {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != ENOENT) {
            return fail(std::error_code(err, std::generic_category()));
        }
    }

#if defined(_SC_AVPHYS_PAGES)
    errno = 0;
    const long pages = ::sysconf(_SC_AVPHYS_PAGES);
    if (pages > 0) {
        const auto ps = page_size_bytes();
        if (!ps) {
            return fail(ps.error());
        }
        const auto u_pages = static_cast<std::uint64_t>(pages);
        if (u_pages > (std::numeric_limits<std::uint64_t>::max)() / (*ps)) {
            return fail(errc::value_too_large);
        }
        return u_pages * (*ps);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0 && saved_errno != EINVAL) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
}

inline result<memory_common::swap_usage> swap_status() {
    FILE* fp = std::fopen("/proc/meminfo", "r");
    if (fp != nullptr) {
        char line[256];
        std::uint64_t swap_total = 0U;
        std::uint64_t swap_free = 0U;
        bool found_total = false;
        bool found_free = false;
        while (std::fgets(line, static_cast<int>(sizeof(line)), fp) !=
               nullptr) {
            if (!found_total) {
                auto parsed = parse_meminfo_kb_line(line, "SwapTotal:");
                if (parsed) {
                    swap_total = *parsed;
                    found_total = true;
                } else if (parsed.error() != make_error_code(errc::not_found)) {
                    std::fclose(fp);
                    return fail(parsed.error());
                }
            }
            if (!found_free) {
                auto parsed = parse_meminfo_kb_line(line, "SwapFree:");
                if (parsed) {
                    swap_free = *parsed;
                    found_free = true;
                } else if (parsed.error() != make_error_code(errc::not_found)) {
                    std::fclose(fp);
                    return fail(parsed.error());
                }
            }
        }
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (found_total && found_free) {
            memory_common::swap_usage usage {};
            usage.total_bytes = swap_total;
            usage.free_bytes = swap_free;
            return usage;
        }
    } else {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != ENOENT) {
            return fail(std::error_code(err, std::generic_category()));
        }
    }
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
    const auto total = physical_memory_bytes();
    if (!total) {
        return fail(total.error());
    }
    const auto avail = available_memory_bytes();
    if (!avail) {
        return fail(avail.error());
    }
    if (*avail > *total) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*total - *avail, *total);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
