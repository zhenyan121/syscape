#ifndef SYSCAPE_DETAIL_RESOURCE_HURD_HPP
#define SYSCAPE_DETAIL_RESOURCE_HURD_HPP

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
    double samples[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(samples, 3) == 3) {
        if (samples[0] < 0.0 || samples[1] < 0.0 || samples[2] < 0.0) {
            return fail(errc::malformed_data);
        }
        resource_common::load_samples result {};
        result.one_minute = samples[0];
        result.five_minute = samples[1];
        result.fifteen_minute = samples[2];
        return result;
    }
    return fail(errc::not_supported);
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
    FILE* fp = std::fopen("/proc/loadavg", "r");
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
    char buf[128];
    if (std::fgets(buf, static_cast<int>(sizeof(buf)), fp) != nullptr) {
        unsigned int run = 0;
        unsigned int total = 0;
        const int parsed = std::sscanf(buf, "%*f %*f %*f %u/%u", &run, &total);
        std::fclose(fp);
        if (parsed == 2 && total > 0) {
            return static_cast<std::uint64_t>(total);
        }
        return fail(errc::malformed_data);
    }
    const int read_err = std::ferror(fp) ? errno : 0;
    std::fclose(fp);
    if (read_err != 0) {
        if (read_err == EACCES || read_err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(read_err, std::generic_category()));
    }
    return fail(errc::malformed_data);
}

inline result<std::uint64_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    errno = 0;
    const long limit = ::sysconf(_SC_OPEN_MAX);
    if (limit > 0) {
        return static_cast<std::uint64_t>(limit);
    }
    const int saved_errno = errno;
    if (saved_errno == EACCES || saved_errno == EPERM) {
        return fail(errc::permission_denied);
    }
    if (saved_errno != 0) {
        return fail(std::error_code(saved_errno, std::generic_category()));
    }
    return fail(errc::not_supported);
}

inline result<std::uint64_t> handle_limit() {
    return fail(errc::not_supported);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
