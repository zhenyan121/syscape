#ifndef SYSCAPE_DETAIL_PROCESS_NUTTX_HPP
#define SYSCAPE_DETAIL_PROCESS_NUTTX_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <sched.h>
#include <string>
#include <system_error>
#include <utility>
#include <unistd.h>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

inline result<std::uint32_t> nuttx_process_identifier(pid_t value) {
    if constexpr (std::numeric_limits<pid_t>::is_signed) {
        if (value < 0) {
            return fail(errc::not_found);
        }
    }
    const auto converted = static_cast<std::uint64_t>(value);
    if (converted > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(converted);
}

inline result<std::uint32_t> process_id() {
    return nuttx_process_identifier(::getpid());
}

inline result<std::uint32_t> parent_process_id() {
    return nuttx_process_identifier(::getppid());
}

inline result<std::string> executable_path() {
    return fail(errc::not_supported);
}
inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<std::string> working_directory() {
    std::size_t size = 128U;
    constexpr std::size_t maximum = 1024U * 1024U;
    while (size <= maximum) {
        std::vector<char> buffer(size);
        errno = 0;
        if (::getcwd(buffer.data(), buffer.size()) != nullptr) {
            return std::string(buffer.data());
        }
        const int error = errno;
        if (error == ERANGE) {
            size *= 2U;
            continue;
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<process_common::cpu_time_usage> cpu_time() {
    // NuttX times() currently zero-fills all per-process accounting fields.
    return fail(errc::not_supported);
}
inline result<std::chrono::system_clock::time_point> start_time() {
    return fail(errc::not_supported);
}
inline result<process_common::memory_usage_snapshot> memory_usage() {
    return fail(errc::not_supported);
}
inline result<std::uint32_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<int> priority() {
    struct sched_param parameters {};
    errno = 0;
    if (::sched_getparam(0, &parameters) != 0) {
        const int error = errno;
        if (error == ENOSYS || error == EINVAL) {
            return fail(errc::not_supported);
        }
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    return parameters.sched_priority >= 0
               ? result<int>(parameters.sched_priority)
               : fail(errc::malformed_data);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    errno = 0;
    if (::sched_getaffinity(0, sizeof(mask), &mask) != 0) {
        const int error = errno;
        if (error == ENOSYS || error == EINVAL) {
            return fail(errc::not_supported);
        }
        return error != 0
                   ? fail(std::error_code(error, std::generic_category()))
                   : fail(errc::io_error);
    }
    std::vector<std::uint32_t> processors;
    for (std::size_t index = 0U; index < static_cast<std::size_t>(CPU_SETSIZE);
         ++index) {
        if (CPU_ISSET(index, &mask)) {
            if (index > (std::numeric_limits<std::uint32_t>::max)()) {
                return fail(errc::value_too_large);
            }
            processors.push_back(static_cast<std::uint32_t>(index));
        }
    }
    return !processors.empty()
               ? result<std::vector<std::uint32_t>>(std::move(processors))
               : fail(errc::malformed_data);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource resource) {
    if (resource != process_common::limit_resource::open_files &&
        resource != process_common::limit_resource::stack_size) {
        // NuttX's getrlimit stub returns zero-filled records for other kinds.
        return fail(errc::not_supported);
    }
    return process_posix::resource_limit(resource);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
