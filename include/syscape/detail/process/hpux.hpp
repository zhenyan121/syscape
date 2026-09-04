#ifndef SYSCAPE_DETAIL_PROCESS_HPUX_HPP
#define SYSCAPE_DETAIL_PROCESS_HPUX_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/resource.h>
#include <sys/times.h>

#if defined(__has_include)
#if __has_include(<sys/pstat.h>)
#include <sys/pstat.h>
#define SYSCAPE_HAS_HPUX_PSTAT 1
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

enum class page_count_add_result { success, negative, overflow };

template <typename Integer>
inline page_count_add_result add_page_count(std::uint64_t& total,
                                            Integer pages) {
    if (pages < 0) {
        return page_count_add_result::negative;
    }
    const auto value = static_cast<std::uint64_t>(pages);
    if (value > UINT64_MAX - total) {
        return page_count_add_result::overflow;
    }
    total += value;
    return page_count_add_result::success;
}

inline result<std::uint32_t> process_id() {
    return static_cast<std::uint32_t>(::getpid());
}

inline result<std::uint32_t> parent_process_id() {
    return static_cast<std::uint32_t>(::getppid());
}

inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<std::string> working_directory() {
    std::size_t size = 1024U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        errno = 0;
        if (::getcwd(&buf[0], size) != nullptr) {
            buf.resize(std::strlen(buf.c_str()));
            return buf;
        }
        const int err = errno;
        if (err == ERANGE) {
            size *= 2U;
            continue;
        }
        if (err == ENOENT) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    return fail(errc::value_too_large);
}

inline result<std::chrono::nanoseconds>
clock_ticks_to_nanoseconds(std::uint64_t ticks, long ticks_per_second) {
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    if (ticks_per_second <= 0 ||
        static_cast<std::uint64_t>(ticks_per_second) >
            (std::numeric_limits<std::uint64_t>::max)() /
                nanoseconds_per_second) {
        return fail(errc::not_supported);
    }
    const std::uint64_t rate = static_cast<std::uint64_t>(ticks_per_second);
    const std::uint64_t whole_seconds = ticks / rate;
    const std::uint64_t maximum_nanoseconds =
        static_cast<std::uint64_t>((std::chrono::nanoseconds::max)().count());
    if (whole_seconds > maximum_nanoseconds / nanoseconds_per_second) {
        return fail(errc::value_too_large);
    }
    const std::uint64_t remainder_nanoseconds =
        (ticks % rate) * nanoseconds_per_second / rate;
    const std::uint64_t whole_nanoseconds =
        whole_seconds * nanoseconds_per_second;
    if (whole_nanoseconds > maximum_nanoseconds - remainder_nanoseconds) {
        return fail(errc::value_too_large);
    }
    using nanoseconds_rep = std::chrono::nanoseconds::rep;
    return std::chrono::nanoseconds(static_cast<nanoseconds_rep>(
        whole_nanoseconds + remainder_nanoseconds));
}

inline result<process_common::cpu_time_usage> cpu_time() {
    struct ::tms t {};
    errno = 0;
    if (::times(&t) == static_cast<clock_t>(-1)) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    errno = 0;
    const long clk = ::sysconf(_SC_CLK_TCK);
    if (clk <= 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(clk == 0 ? errc::malformed_data : errc::not_supported);
    }
    if (t.tms_utime < static_cast<clock_t>(0) ||
        t.tms_stime < static_cast<clock_t>(0)) {
        return fail(errc::malformed_data);
    }
    const auto user = clock_ticks_to_nanoseconds(
        static_cast<std::uint64_t>(t.tms_utime), clk);
    if (!user) {
        return fail(user.error());
    }
    const auto system = clock_ticks_to_nanoseconds(
        static_cast<std::uint64_t>(t.tms_stime), clk);
    if (!system) {
        return fail(system.error());
    }
    process_common::cpu_time_usage usage;
    usage.user = *user;
    usage.system = *system;
    return usage;
}

inline result<std::chrono::system_clock::time_point>
start_seconds_to_time_point(std::uint64_t seconds) {
    using clock = std::chrono::system_clock;
    const auto maximum_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (seconds > static_cast<std::uint64_t>(maximum_seconds)) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(seconds)));
}

inline result<std::chrono::system_clock::time_point> start_time() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_status pss {};
    errno = 0;
    const int count = ::pstat_getproc(&pss, sizeof(pss), 0, ::getpid());
    if (count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno == ESRCH || saved_errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (count == 0) {
        return fail(errc::not_found);
    }
    if (count != 1) {
        return fail(errc::malformed_data);
    }
    if (pss.pst_start > 0) {
        return start_seconds_to_time_point(
            static_cast<std::uint64_t>(pss.pst_start));
    }
    return fail(errc::malformed_data);
#else
    return fail(errc::not_supported);
#endif
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_status pss {};
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
    errno = 0;
    const int process_count = ::pstat_getproc(&pss, sizeof(pss), 0, ::getpid());
    if (process_count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno == ESRCH || saved_errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (process_count == 0) {
        return fail(errc::not_found);
    }
    if (process_count != 1) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t page_size = static_cast<std::uint64_t>(pst.page_size);
    std::uint64_t rss_pages = 0U;
    const page_count_add_result rss_results[] = {
        add_page_count(rss_pages, pss.pst_rssize),
        add_page_count(rss_pages, pss.pst_shmsize),
        add_page_count(rss_pages, pss.pst_mmsize),
        add_page_count(rss_pages, pss.pst_usize),
        add_page_count(rss_pages, pss.pst_iosize)};
    for (page_count_add_result result : rss_results) {
        if (result == page_count_add_result::negative) {
            return fail(errc::malformed_data);
        }
        if (result == page_count_add_result::overflow) {
            return fail(errc::value_too_large);
        }
    }
    if (rss_pages > UINT64_MAX / page_size) {
        return fail(errc::value_too_large);
    }
    std::uint64_t vsize_pages = 0U;
    const page_count_add_result vsize_results[] = {
        add_page_count(vsize_pages, pss.pst_vtsize),
        add_page_count(vsize_pages, pss.pst_vdsize),
        add_page_count(vsize_pages, pss.pst_vssize),
        add_page_count(vsize_pages, pss.pst_vshmsize),
        add_page_count(vsize_pages, pss.pst_vmmsize),
        add_page_count(vsize_pages, pss.pst_vusize),
        add_page_count(vsize_pages, pss.pst_viosize)
#if defined(__ia64) || defined(__ia64__)
            ,
        add_page_count(vsize_pages, pss.pst_vrsesize)
#endif
    };
    for (page_count_add_result result : vsize_results) {
        if (result == page_count_add_result::negative) {
            return fail(errc::malformed_data);
        }
        if (result == page_count_add_result::overflow) {
            return fail(errc::value_too_large);
        }
    }
    if (vsize_pages > UINT64_MAX / page_size) {
        return fail(errc::value_too_large);
    }
    process_common::memory_usage_snapshot mem {};
    mem.resident_bytes = rss_pages * page_size;
    mem.virtual_bytes = vsize_pages * page_size;
    return mem;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<int> priority() {
    return process_posix::priority(-20, 19);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource resource) {
    return process_posix::resource_limit(resource);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
