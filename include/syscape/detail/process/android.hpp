#ifndef SYSCAPE_DETAIL_PROCESS_ANDROID_HPP
#define SYSCAPE_DETAIL_PROCESS_ANDROID_HPP

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>

#include <syscape/detail/android/file.hpp>
#include <syscape/detail/linux/process_metrics.hpp>
#include <syscape/detail/os/android.hpp>
#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

using linux_process_metrics::compose_start_time;
using linux_process_metrics::ticks_to_nanoseconds;

inline result<std::uint32_t> process_id() {
    const pid_t pid = ::getpid();
    if (pid <= 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return static_cast<std::uint32_t>(pid);
}

inline result<std::uint32_t> parent_process_id() {
    const pid_t ppid = ::getppid();
    if (ppid < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return static_cast<std::uint32_t>(ppid);
}

inline result<std::string> executable_path() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 64U * 1024U;
    for (;;) {
        const ssize_t count =
            ::readlink("/proc/self/exe", buffer.data(), buffer.size());
        if (count >= 0 && static_cast<std::size_t>(count) < buffer.size()) {
            std::string path(buffer.data(), static_cast<std::size_t>(count));
            if (path.empty() || path.front() != '/') {
                return fail(errc::malformed_data);
            }
            return path;
        }
        if (count < 0) {
            if (errno != EINTR) {
                if (errno == EACCES || errno == EPERM) {
                    return fail(errc::permission_denied);
                }
                return fail(std::error_code(errno, std::generic_category()));
            }
            continue;
        }
        if (buffer.size() > maximum_size / 2U) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::vector<std::string>>
parse_command_line(std::string_view input) {
    if (input.empty()) {
        return std::vector<std::string>();
    }
    if (input.back() != '\0') {
        return fail(errc::malformed_data);
    }
    input.remove_suffix(1U);

    std::vector<std::string> arguments;
    std::size_t start = 0U;
    for (;;) {
        const std::size_t end = input.find('\0', start);
        if (end == std::string_view::npos) {
            arguments.emplace_back(input.substr(start));
            break;
        }
        arguments.emplace_back(input.substr(start, end - start));
        start = end + 1U;
    }
    return arguments;
}

inline result<std::vector<std::string>> command_line() {
    const auto content = android::read_text_file("/proc/self/cmdline");
    if (!content) {
        return fail(content.error());
    }
    return parse_command_line(*content);
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            std::string path(buffer.data());
            if (path.empty() || path.front() != '/') {
                return fail(errc::malformed_data);
            }
            return path;
        }
        if (errno != ERANGE) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::uint64_t> parse_unsigned_token(std::string_view token) {
    if (token.empty())
        return fail(errc::malformed_data);
    std::uint64_t val = 0U;
    const auto r =
        std::from_chars(token.data(), token.data() + token.size(), val);
    if (r.ec == std::errc::result_out_of_range)
        return fail(errc::value_too_large);
    if (r.ec != std::errc() || r.ptr != token.data() + token.size())
        return fail(errc::malformed_data);
    return val;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    const auto content = android::read_text_file("/proc/self/stat");
    if (!content) {
        return fail(content.error());
    }

    const std::size_t name_end = content->rfind(')');
    if (name_end == std::string_view::npos ||
        name_end + 1U >= content->size()) {
        return fail(errc::malformed_data);
    }

    const std::string_view remainder =
        std::string_view(*content).substr(name_end + 1U);
    std::array<std::string_view, 22> fields {};
    std::size_t found = 0U;
    std::size_t pos = 0U;

    while (pos < remainder.size() && found < 22U) {
        while (pos < remainder.size() &&
               (remainder[pos] == ' ' || remainder[pos] == '\t'))
            ++pos;
        if (pos >= remainder.size())
            break;
        const std::size_t start = pos;
        while (pos < remainder.size() && remainder[pos] != ' ' &&
               remainder[pos] != '\t' && remainder[pos] != '\n')
            ++pos;
        fields[found++] = remainder.substr(start, pos - start);
    }

    if (found < 20U) {
        return fail(errc::malformed_data);
    }

    // fields[19] is field 22 (starttime in clock ticks after boot)
    const auto start_ticks = parse_unsigned_token(fields[19]);
    if (!start_ticks) {
        return fail(start_ticks.error());
    }

    errno = 0;
    const long ticks_per_sec = ::sysconf(_SC_CLK_TCK);
    if (ticks_per_sec <= 0) {
        return errno != 0
                   ? result<std::chrono::system_clock::time_point>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::chrono::system_clock::time_point>(
                         fail(errc::not_supported));
    }

    const auto boot = os_backend::boot_time();
    if (!boot) {
        return fail(boot.error());
    }

    const auto age = ticks_to_nanoseconds(*start_ticks, ticks_per_sec);
    if (!age) {
        return fail(age.error());
    }

    return compose_start_time(*boot, *age);
}

inline result<process_common::cpu_time_usage> cpu_time() {
    struct rusage usage {};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    process_common::cpu_time_usage dur {};
    dur.user = std::chrono::seconds(usage.ru_utime.tv_sec) +
               std::chrono::microseconds(usage.ru_utime.tv_usec);
    dur.system = std::chrono::seconds(usage.ru_stime.tv_sec) +
                 std::chrono::microseconds(usage.ru_stime.tv_usec);
    return dur;
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const auto content = android::read_text_file("/proc/self/statm");
    if (!content) {
        return fail(content.error());
    }

    std::string_view text = *content;
    android::strip_trailing_newlines(text);
    std::size_t sp = text.find(' ');
    if (sp == std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    const auto virt_pages = parse_unsigned_token(text.substr(0U, sp));
    if (!virt_pages)
        return fail(virt_pages.error());

    while (sp < text.size() && text[sp] == ' ')
        ++sp;
    const std::size_t sp2 = text.find(' ', sp);
    const auto res_pages = parse_unsigned_token(
        sp2 == std::string_view::npos ? text.substr(sp)
                                      : text.substr(sp, sp2 - sp));
    if (!res_pages)
        return fail(res_pages.error());

    errno = 0;
    const long page_sz = ::sysconf(_SC_PAGESIZE);
    if (page_sz <= 0) {
        return errno != 0
                   ? result<process_common::memory_usage_snapshot>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<process_common::memory_usage_snapshot>(
                         fail(errc::not_supported));
    }
    const auto page_size = static_cast<std::uint64_t>(page_sz);
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (*virt_pages > max_u64 / page_size || *res_pages > max_u64 / page_size) {
        return fail(errc::value_too_large);
    }

    process_common::memory_usage_snapshot extents {};
    extents.virtual_bytes = *virt_pages * page_size;
    extents.resident_bytes = *res_pages * page_size;
    return extents;
}

inline result<std::uint32_t> thread_count() {
    const auto content = android::read_text_file("/proc/self/stat");
    if (!content) {
        return fail(content.error());
    }

    const std::size_t name_end = content->rfind(')');
    if (name_end == std::string_view::npos ||
        name_end + 1U >= content->size()) {
        return fail(errc::malformed_data);
    }

    const std::string_view remainder =
        std::string_view(*content).substr(name_end + 1U);
    std::array<std::string_view, 20> fields {};
    std::size_t found = 0U;
    std::size_t pos = 0U;

    while (pos < remainder.size() && found < 20U) {
        while (pos < remainder.size() &&
               (remainder[pos] == ' ' || remainder[pos] == '\t'))
            ++pos;
        if (pos >= remainder.size())
            break;
        const std::size_t start = pos;
        while (pos < remainder.size() && remainder[pos] != ' ' &&
               remainder[pos] != '\t' && remainder[pos] != '\n')
            ++pos;
        fields[found++] = remainder.substr(start, pos - start);
    }

    if (found < 18U) {
        return fail(errc::malformed_data);
    }

    // fields[17] is field 20 (num_threads)
    const auto threads = parse_unsigned_token(fields[17]);
    if (!threads)
        return fail(threads.error());
    if (*threads == 0U ||
        *threads > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(*threads);
}

inline result<std::int32_t> priority() {
    errno = 0;
    const int prio = ::getpriority(PRIO_PROCESS, 0);
    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return prio;
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource kind) {
    int res_kind = 0;
    switch (kind) {
    case process_common::limit_resource::cpu_time:
        res_kind = RLIMIT_CPU;
        break;
    case process_common::limit_resource::file_size:
        res_kind = RLIMIT_FSIZE;
        break;
    case process_common::limit_resource::core_file_size:
        res_kind = RLIMIT_CORE;
        break;
    case process_common::limit_resource::open_files:
        res_kind = RLIMIT_NOFILE;
        break;
    case process_common::limit_resource::stack_size:
        res_kind = RLIMIT_STACK;
        break;
    case process_common::limit_resource::address_space:
        res_kind = RLIMIT_AS;
        break;
    default:
        return fail(errc::not_supported);
    }

    struct rlimit lim {};
    if (::getrlimit(res_kind, &lim) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    process_common::resource_limit_snapshot snapshot {};
    if (lim.rlim_cur == RLIM_INFINITY) {
        snapshot.soft.unlimited = true;
    } else {
        snapshot.soft.amount = static_cast<std::uint64_t>(lim.rlim_cur);
    }
    if (lim.rlim_max == RLIM_INFINITY) {
        snapshot.hard.unlimited = true;
    } else {
        snapshot.hard.amount = static_cast<std::uint64_t>(lim.rlim_max);
    }
    return snapshot;
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
