#ifndef SYSCAPE_DETAIL_PROCESS_HURD_HPP
#define SYSCAPE_DETAIL_PROCESS_HURD_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/resource.h>
#include <sys/times.h>

#if defined(__has_include)
#if __has_include(<mach/mach.h>) && __has_include(<mach/task_info.h>)
#include <mach/mach.h>
#include <mach/mach_types.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <mach/vm_map.h>
#define SYSCAPE_HURD_HAS_MACH 1
#endif
#endif

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id() {
    return static_cast<std::uint32_t>(::getpid());
}

inline result<std::uint32_t> parent_process_id() {
    return static_cast<std::uint32_t>(::getppid());
}

inline result<std::string> executable_path() {
    std::size_t size = 256U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        errno = 0;
        const ssize_t len = ::readlink("/proc/self/exe", &buf[0], size);
        if (len > 0) {
            if (static_cast<std::size_t>(len) < size) {
                buf.resize(static_cast<std::size_t>(len));
                return buf;
            }
            size *= 2U;
            continue;
        }
        const int err = errno;
        if (err == EINTR) {
            continue;
        }
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0) {
            return fail(std::error_code(err, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    return fail(errc::value_too_large);
}

inline result<std::vector<std::string>> command_line() {
    FILE* fp = std::fopen("/proc/self/cmdline", "rb");
    if (fp == nullptr) {
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    std::vector<char> buffer;
    char chunk[512];
    std::size_t read_bytes = 0;
    constexpr std::size_t max_cmdline_size = 2U * 1024U * 1024U;
    while ((read_bytes = std::fread(chunk, 1, sizeof(chunk), fp)) > 0) {
        if (buffer.size() + read_bytes > max_cmdline_size) {
            std::fclose(fp);
            return fail(errc::value_too_large);
        }
        buffer.insert(buffer.end(), chunk, chunk + read_bytes);
    }
    const int read_err = std::ferror(fp) ? errno : 0;
    std::fclose(fp);
    if (read_err != 0) {
        if (read_err == EACCES || read_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(read_err, std::generic_category()));
    }

    std::vector<std::string> args;
    std::size_t start = 0;
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        if (buffer[i] == '\0') {
            args.emplace_back(buffer.data() + start, i - start);
            start = i + 1;
        }
    }
    if (start < buffer.size()) {
        args.emplace_back(buffer.data() + start, buffer.size() - start);
    }
    return args;
}

inline result<std::string> working_directory() {
    std::size_t size = 1024U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        errno = 0;
        if (::getcwd(&buf[0], size) != nullptr) {
            buf.resize(std::strlen(buf.c_str()));
            if (buf.empty() || buf.front() != '/') {
                return fail(errc::malformed_data);
            }
            return buf;
        }
        const int err = errno;
        if (err == EINTR) {
            continue;
        }
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
    }
    errno = 0;
    const long clk = ::sysconf(_SC_CLK_TCK);
    if (clk <= 0) {
        const int saved_errno = errno;
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
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

inline result<std::chrono::system_clock::time_point> start_time() {
    return fail(errc::not_supported);
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
#if defined(SYSCAPE_HURD_HAS_MACH)
    struct ::task_basic_info info {};
    ::mach_msg_type_number_t count = TASK_BASIC_INFO_COUNT;
    const kern_return_t kr =
        ::task_info(::mach_task_self(), TASK_BASIC_INFO,
                    reinterpret_cast<task_info_t>(&info), &count);
    if (kr == KERN_SUCCESS && count >= TASK_BASIC_INFO_COUNT) {
        if (info.resident_size > 0 || info.virtual_size > 0) {
            process_common::memory_usage_snapshot mem {};
            mem.resident_bytes = static_cast<std::uint64_t>(info.resident_size);
            mem.virtual_bytes = static_cast<std::uint64_t>(info.virtual_size);
            return mem;
        }
    }
#endif

    FILE* fp = std::fopen("/proc/self/statm", "r");
    if (fp != nullptr) {
        unsigned long vpages = 0;
        unsigned long rpages = 0;
        const int read_count = std::fscanf(fp, "%lu %lu", &vpages, &rpages);
        const int read_err = std::ferror(fp) ? errno : 0;
        std::fclose(fp);
        if (read_err != 0) {
            if (read_err == EACCES || read_err == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(read_err, std::generic_category()));
        }
        if (read_count == 2) {
            errno = 0;
            const long ps = ::sysconf(_SC_PAGESIZE);
            if (ps > 0) {
                const auto page_size = static_cast<std::uint64_t>(ps);
                if (vpages > (std::numeric_limits<std::uint64_t>::max)() /
                                 page_size ||
                    rpages > (std::numeric_limits<std::uint64_t>::max)() /
                                 page_size) {
                    return fail(errc::value_too_large);
                }
                process_common::memory_usage_snapshot mem {};
                mem.virtual_bytes =
                    static_cast<std::uint64_t>(vpages) * page_size;
                mem.resident_bytes =
                    static_cast<std::uint64_t>(rpages) * page_size;
                if (mem.virtual_bytes > 0 || mem.resident_bytes > 0) {
                    return mem;
                }
            }
        }
        return fail(errc::malformed_data);
    }

    const int err = errno;
    if (err == EACCES || err == EPERM) {
        return fail(errc::permission_denied);
    }
    if (err == ENOENT) {
        return fail(errc::not_supported);
    }
    return fail(std::error_code(err, std::generic_category()));
}

inline result<std::uint32_t> thread_count() {
#if defined(SYSCAPE_HURD_HAS_MACH)
    ::thread_array_t thread_list = nullptr;
    ::mach_msg_type_number_t count = 0U;
    const kern_return_t kr =
        ::task_threads(::mach_task_self(), &thread_list, &count);
    if (kr == KERN_SUCCESS) {
        for (::mach_msg_type_number_t i = 0U; i < count; ++i) {
            ::mach_port_deallocate(::mach_task_self(), thread_list[i]);
        }
        if (thread_list != nullptr && count > 0U) {
            ::vm_deallocate(::mach_task_self(),
                            reinterpret_cast<vm_address_t>(thread_list),
                            static_cast<vm_size_t>(count * sizeof(::thread_t)));
        }
        if (count > 0U) {
            return static_cast<std::uint32_t>(count);
        }
    }
#endif

    FILE* fp = std::fopen("/proc/self/status", "r");
    if (fp == nullptr) {
        const int err = errno;
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    char line[256];
    bool found = false;
    bool invalid_format = false;
    unsigned int tc = 0;
    while (std::fgets(line, static_cast<int>(sizeof(line)), fp) != nullptr) {
        if (std::strncmp(line, "Threads:", 8) == 0) {
            if (std::sscanf(line + 8, "%u", &tc) == 1 && tc > 0) {
                found = true;
            } else {
                invalid_format = true;
            }
            break;
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
    if (invalid_format) {
        return fail(errc::malformed_data);
    }
    if (found) {
        return static_cast<std::uint32_t>(tc);
    }
    return fail(errc::not_found);
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
