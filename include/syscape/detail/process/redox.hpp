#ifndef SYSCAPE_DETAIL_PROCESS_REDOX_HPP
#define SYSCAPE_DETAIL_PROCESS_REDOX_HPP

#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fcntl.h>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

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
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> command_line() {
    return fail(errc::not_supported);
}

inline result<std::string> working_directory() {
    std::size_t size = 256U;
    constexpr std::size_t max_size = 1024U * 1024U;
    while (size <= max_size) {
        std::vector<char> buf(size);
        errno = 0;
        if (::getcwd(buf.data(), size) != nullptr) {
            return std::string(buf.data());
        }
        const int err = errno;
        if (err == ERANGE) {
            size *= 2U;
            continue;
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0) {
            return fail(std::error_code(err, std::generic_category()));
        }
        return fail(errc::io_error);
    }
    return fail(errc::value_too_large);
}

inline result<process_common::cpu_time_usage> cpu_time() {
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
    return process_posix::priority(-20, 19);
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    return fail(errc::not_supported);
}

inline result<process_common::resource_limit_snapshot>
resource_limit(process_common::limit_resource) {
    return fail(errc::not_supported);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
