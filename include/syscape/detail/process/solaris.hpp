#ifndef SYSCAPE_DETAIL_PROCESS_SOLARIS_HPP
#define SYSCAPE_DETAIL_PROCESS_SOLARIS_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#if defined(__has_include)
#if __has_include(<procfs.h>)
#include <procfs.h>
#define SYSCAPE_HAS_PROCFS 1
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::chrono::nanoseconds>
timeval_to_nanoseconds(std::int64_t seconds, std::int64_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t nanoseconds_per_second = 1000000000U;
    constexpr std::uint64_t nanoseconds_per_microsecond = 1000U;
    const std::uint64_t whole = static_cast<std::uint64_t>(seconds);
    const std::uint64_t fraction =
        static_cast<std::uint64_t>(microseconds) * nanoseconds_per_microsecond;
    const std::uint64_t maximum =
        static_cast<std::uint64_t>((std::chrono::nanoseconds::max)().count());
    if (whole > maximum / nanoseconds_per_second ||
        whole * nanoseconds_per_second > maximum - fraction) {
        return fail(errc::value_too_large);
    }
    return std::chrono::nanoseconds(whole * nanoseconds_per_second + fraction);
}

inline result<std::uint32_t> process_id() {
    return static_cast<std::uint32_t>(::getpid());
}

inline result<std::uint32_t> parent_process_id() {
    return static_cast<std::uint32_t>(::getppid());
}

inline result<std::string> read_symlink_exact(const char* path) {
    std::size_t size = 512U;
    for (int attempts = 0; attempts < 4; ++attempts) {
        std::string buffer(size, '\0');
        const ssize_t len = ::readlink(path, &buffer[0], size);
        if (len < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (static_cast<std::size_t>(len) < size) {
            buffer.resize(static_cast<std::size_t>(len));
            return buffer;
        }
        size *= 2U;
    }
    return fail(errc::malformed_data);
}

inline result<std::string> executable_path() {
    return read_symlink_exact("/proc/self/path/a.out");
}

inline result<std::string> working_directory() {
    const auto cwd_link = read_symlink_exact("/proc/self/path/cwd");
    if (cwd_link) {
        return *cwd_link;
    }
    std::size_t size = 1024U;
    for (int attempts = 0; attempts < 4; ++attempts) {
        std::string buffer(size, '\0');
        if (::getcwd(&buffer[0], size) != nullptr) {
            const std::size_t null_pos = buffer.find('\0');
            if (null_pos != std::string::npos) {
                buffer.resize(null_pos);
            }
            return buffer;
        }
        if (errno == ERANGE) {
            size *= 2U;
            continue;
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return fail(errc::malformed_data);
}

#if defined(SYSCAPE_HAS_PROCFS)

inline result<::psinfo_t> read_self_psinfo() {
    const int fd = ::open("/proc/self/psinfo", O_RDONLY);
    if (fd < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    ::psinfo_t info {};
    char* target = reinterpret_cast<char*>(&info);
    std::size_t total_read = 0U;
    const std::size_t required = sizeof(info);
    while (total_read < required) {
        const ssize_t bytes =
            ::read(fd, target + total_read, required - total_read);
        if (bytes > 0) {
            total_read += static_cast<std::size_t>(bytes);
        } else if (bytes == 0) {
            break;
        } else if (errno == EINTR) {
            continue;
        } else {
            const int saved_errno = errno;
            ::close(fd);
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
    }
    ::close(fd);
    if (total_read != required) {
        return fail(errc::malformed_data);
    }
    return info;
}

#endif // SYSCAPE_HAS_PROCFS

inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<process_common::cpu_time_usage> cpu_time() {
    struct ::rusage ru {};
    if (::getrusage(RUSAGE_SELF, &ru) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const result<std::chrono::nanoseconds> user =
        timeval_to_nanoseconds(static_cast<std::int64_t>(ru.ru_utime.tv_sec),
                               static_cast<std::int64_t>(ru.ru_utime.tv_usec));
    if (!user) {
        return fail(user.error());
    }
    const result<std::chrono::nanoseconds> system =
        timeval_to_nanoseconds(static_cast<std::int64_t>(ru.ru_stime.tv_sec),
                               static_cast<std::int64_t>(ru.ru_stime.tv_usec));
    if (!system) {
        return fail(system.error());
    }
    process_common::cpu_time_usage usage;
    usage.user = *user;
    usage.system = *system;
    return usage;
}

inline result<std::chrono::system_clock::time_point> start_time() {
#if defined(SYSCAPE_HAS_PROCFS)
    const auto info = read_self_psinfo();
    if (!info) {
        return fail(info.error());
    }
    if (info->pr_start.tv_sec <= 0) {
        return fail(errc::malformed_data);
    }
    const auto sec = std::chrono::seconds(info->pr_start.tv_sec);
    const auto nsec = std::chrono::nanoseconds(info->pr_start.tv_nsec);
    return std::chrono::system_clock::time_point(
        std::chrono::duration_cast<std::chrono::system_clock::duration>(sec +
                                                                        nsec));
#else
    return fail(errc::not_supported);
#endif
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
#if defined(SYSCAPE_HAS_PROCFS)
    const auto info = read_self_psinfo();
    if (!info) {
        return fail(info.error());
    }
    process_common::memory_usage_snapshot mem {};
    const auto rss_kib = static_cast<std::uint64_t>(info->pr_rssize);
    const auto virt_kib = static_cast<std::uint64_t>(info->pr_size);
    if (rss_kib > UINT64_MAX / 1024ULL || virt_kib > UINT64_MAX / 1024ULL) {
        return fail(errc::value_too_large);
    }
    mem.resident_bytes = rss_kib * 1024ULL;
    mem.virtual_bytes = virt_kib * 1024ULL;
    return mem;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> thread_count() {
#if defined(SYSCAPE_HAS_PROCFS)
    const auto info = read_self_psinfo();
    if (!info) {
        return fail(info.error());
    }
    if (info->pr_nlwp <= 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(info->pr_nlwp);
#else
    return static_cast<std::uint32_t>(1U);
#endif
}

inline result<int> priority() {
    return process_posix::priority(-20, 20);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource kind) {
    return process_posix::resource_limit(kind);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
