#ifndef SYSCAPE_DETAIL_PROCESS_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_PROCESS_APPLE_MOBILE_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <crt_externs.h>
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/task.h>
#include <mach/task_info.h>
#include <sys/sysctl.h>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> process_id_value(pid_t value, bool zero_is_valid) {
    if (value < 0 || (!zero_is_valid && value == 0)) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> process_id() {
    return process_id_value(::getpid(), false);
}

inline result<std::uint32_t> parent_process_id() {
    return process_id_value(::getppid(), true);
}

inline result<std::string> executable_path() {
    std::vector<char> buffer(1024U);
    std::uint32_t size = static_cast<std::uint32_t>(buffer.size());
    int status = ::_NSGetExecutablePath(buffer.data(), &size);
    if (status == -1) {
        if (size == 0U) {
            return fail(errc::malformed_data);
        }
        buffer.resize(size);
        size = static_cast<std::uint32_t>(buffer.size());
        status = ::_NSGetExecutablePath(buffer.data(), &size);
    }
    if (status != 0) {
        return fail(errc::io_error);
    }

    std::size_t length = 0U;
    while (length < buffer.size() && buffer[length] != '\0') {
        ++length;
    }
    if (length >= buffer.size()) {
        return fail(errc::malformed_data);
    }
    const std::string path(buffer.data(), length);
    return path.empty() || path.front() != '/'
               ? result<std::string>(fail(errc::malformed_data))
               : result<std::string>(path);
}

inline result<std::vector<std::string>> command_line() {
    const int* count_pointer = ::_NSGetArgc();
    char*** argument_storage = ::_NSGetArgv();
    if (count_pointer == nullptr || argument_storage == nullptr ||
        *argument_storage == nullptr) {
        return fail(errc::not_found);
    }

    const int count = *count_pointer;
    if (count < 0) {
        return fail(errc::malformed_data);
    }
    std::vector<std::string> values;
    if (static_cast<std::size_t>(count) > values.max_size()) {
        return fail(errc::resource_exhausted);
    }
    values.resize(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const char* argument = (*argument_storage)[index];
        if (argument == nullptr) {
            return fail(errc::malformed_data);
        }
        values[static_cast<std::size_t>(index)] = std::string(argument);
    }
    return values;
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            const std::string path(buffer.data());
            return path.empty() || path.front() != '/'
                       ? result<std::string>(fail(errc::malformed_data))
                       : result<std::string>(std::move(path));
        }
        if (errno != ERANGE) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() <= maximum_size / 2U ? buffer.size() * 2U
                                                         : maximum_size);
    }
}

inline result<mach_task_basic_info> query_task_basic_info() {
    mach_task_basic_info info {};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    const kern_return_t status =
        ::task_info(::mach_task_self(), MACH_TASK_BASIC_INFO,
                    reinterpret_cast<task_info_t>(&info), &count);
    if (status != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (count != MACH_TASK_BASIC_INFO_COUNT) {
        return fail(errc::malformed_data);
    }
    return info;
}

inline result<std::uint32_t> thread_count() {
    thread_act_array_t thread_list = nullptr;
    mach_msg_type_number_t count = 0U;
    const kern_return_t status =
        ::task_threads(::mach_task_self(), &thread_list, &count);
    if (status != KERN_SUCCESS) {
        return fail(errc::io_error);
    }
    if (thread_list != nullptr && count != 0U) {
        for (mach_msg_type_number_t i = 0U; i < count; ++i) {
            ::mach_port_deallocate(::mach_task_self(), thread_list[i]);
        }
        static_cast<void>(::vm_deallocate(
            ::mach_task_self(), reinterpret_cast<vm_address_t>(thread_list),
            static_cast<vm_size_t>(count) * sizeof(thread_act_t)));
    }
    if (count == 0U) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(count);
}

inline result<std::chrono::nanoseconds>
time_value_to_nanoseconds(integer_t seconds, integer_t microseconds) {
    if (seconds < 0 || microseconds < 0 || microseconds >= 1000000) {
        return fail(errc::malformed_data);
    }
    const std::int64_t seconds_value = static_cast<std::int64_t>(seconds);
    const std::int64_t microseconds_value =
        static_cast<std::int64_t>(microseconds);
    const std::int64_t max_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            (std::chrono::nanoseconds::max)())
            .count();
    if (seconds_value > max_seconds) {
        return fail(errc::value_too_large);
    }
    const auto whole = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::seconds(seconds_value));
    const auto fraction = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::microseconds(microseconds_value));
    if (fraction > (std::chrono::nanoseconds::max)() - whole) {
        return fail(errc::value_too_large);
    }
    return whole + fraction;
}

inline result<process_common::cpu_time_usage> cpu_time() {
    const result<mach_task_basic_info> info = query_task_basic_info();
    if (!info) {
        return fail(info.error());
    }

    const auto user_ns = time_value_to_nanoseconds(
        info->user_time.seconds, info->user_time.microseconds);
    if (!user_ns) {
        return fail(user_ns.error());
    }
    const auto system_ns = time_value_to_nanoseconds(
        info->system_time.seconds, info->system_time.microseconds);
    if (!system_ns) {
        return fail(system_ns.error());
    }

    process_common::cpu_time_usage usage;
    usage.user = *user_ns;
    usage.system = *system_ns;
    return usage;
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const result<mach_task_basic_info> info = query_task_basic_info();
    if (!info) {
        return fail(info.error());
    }

    process_common::memory_usage_snapshot usage;
    usage.resident_bytes = info->resident_size;
    usage.virtual_bytes = info->virtual_size;
    return usage;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, ::getpid()};
    struct ::kinfo_proc info {};
    std::size_t size = sizeof(info);
    if (::sysctl(mib, 4U, &info, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(info)) {
        return fail(errc::malformed_data);
    }

    const struct timeval tv = info.kp_proc.p_starttime;
    if (tv.tv_sec <= 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000) {
        return fail(errc::malformed_data);
    }
    using clock = std::chrono::system_clock;
    const auto max_seconds =
        std::chrono::duration_cast<std::chrono::seconds>(clock::duration::max())
            .count();
    if (tv.tv_sec > max_seconds) {
        return fail(errc::value_too_large);
    }
    const auto whole = std::chrono::duration_cast<clock::duration>(
        std::chrono::seconds(tv.tv_sec));
    const auto fraction = std::chrono::duration_cast<clock::duration>(
        std::chrono::microseconds(tv.tv_usec));
    if (fraction > clock::duration::max() - whole) {
        return fail(errc::value_too_large);
    }
    return clock::time_point(whole + fraction);
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
