#ifndef SYSCAPE_DETAIL_PROCESS_HAIKU_HPP
#define SYSCAPE_DETAIL_PROCESS_HAIKU_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sys/times.h>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#if __has_include(<image.h>)
#include <image.h>
#define SYSCAPE_HAS_HAIKU_IMAGE_H 1
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
#if defined(SYSCAPE_HAS_HAIKU_IMAGE_H)
    int32 cookie = 0;
    ::image_info iinfo {};
    while (::get_next_image_info(::getpid(), &cookie, &iinfo) == B_OK) {
        if (iinfo.type == B_APP_IMAGE && iinfo.name[0] == '/') {
            return std::string(iinfo.name);
        }
    }
#endif
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
    return fail(errc::malformed_data);
}

inline result<process_common::cpu_time_usage> cpu_time() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::team_usage_info uinfo {};
    if (::get_team_usage_info(::getpid(), B_TEAM_USAGE_SELF, &uinfo) == B_OK) {
        process_common::cpu_time_usage usage;
        usage.user = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::microseconds(uinfo.user_time));
        usage.system = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::microseconds(uinfo.kernel_time));
        return usage;
    }
#endif
    struct ::tms t {};
    if (::times(&t) != static_cast<clock_t>(-1)) {
        const long clk = ::sysconf(_SC_CLK_TCK);
        if (clk > 0) {
            process_common::cpu_time_usage usage;
            usage.user = std::chrono::nanoseconds(
                static_cast<std::uint64_t>(t.tms_utime) * 1000000000ULL /
                static_cast<std::uint64_t>(clk));
            usage.system = std::chrono::nanoseconds(
                static_cast<std::uint64_t>(t.tms_stime) * 1000000000ULL /
                static_cast<std::uint64_t>(clk));
            return usage;
        }
    }
    return fail(errc::not_supported);
}

inline result<std::chrono::system_clock::time_point> start_time() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::system_info sinfo {};
    if (::get_system_info(&sinfo) != B_OK || sinfo.boot_time <= 0) {
        return fail(errc::not_supported);
    }
    ::team_info tinfo {};
    if (::get_team_info(::getpid(), &tinfo) == B_OK && tinfo.start_time > 0) {
        const bigtime_t wall_start_us = sinfo.boot_time + tinfo.start_time;
        const auto us = std::chrono::microseconds(wall_start_us);
        return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                us));
    }
#endif
    return fail(errc::not_supported);
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ssize_t cookie = 0;
    ::area_info ainfo {};
    std::uint64_t ram = 0;
    std::uint64_t virt = 0;
    while (::get_next_area_info(::getpid(), &cookie, &ainfo) == B_OK) {
        ram += ainfo.ram_size;
        virt += ainfo.size;
    }
    process_common::memory_usage_snapshot mem;
    mem.resident_bytes = ram;
    mem.virtual_bytes = virt;
    return mem;
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint32_t> thread_count() {
#if defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::team_info tinfo {};
    if (::get_team_info(::getpid(), &tinfo) == B_OK) {
        return static_cast<std::uint32_t>(tinfo.thread_count);
    }
#endif
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
