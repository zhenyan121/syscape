#ifndef SYSCAPE_DETAIL_RESOURCE_FREEBSD_HPP
#define SYSCAPE_DETAIL_RESOURCE_FREEBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline std::error_code native_error() {
    return std::error_code(errno, std::generic_category());
}

template <typename T>
inline result<std::int64_t> checked_nonnegative(T value) {
    if (value < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::int64_t>(value);
}

inline result<std::int64_t> read_count_sysctl(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return fail(native_error());
    }
    if (size == sizeof(std::int32_t)) {
        std::int32_t narrow = 0;
        std::size_t narrow_size = sizeof(narrow);
        if (::sysctlbyname(name, &narrow, &narrow_size, nullptr, 0U) != 0) {
            return fail(native_error());
        }
        if (narrow_size != sizeof(narrow)) {
            return fail(errc::malformed_data);
        }
        return checked_nonnegative(narrow);
    }
    if (size == sizeof(std::int64_t)) {
        std::int64_t wide = 0;
        std::size_t wide_size = sizeof(wide);
        if (::sysctlbyname(name, &wide, &wide_size, nullptr, 0U) != 0) {
            return fail(native_error());
        }
        if (wide_size != sizeof(wide)) {
            return fail(errc::malformed_data);
        }
        return checked_nonnegative(wide);
    }
    return fail(errc::malformed_data);
}

inline result<resource_common::load_samples> load_average() {
    double samples[3] = {0.0, 0.0, 0.0};
    if (::getloadavg(samples, 3) != 3) {
        return fail(errc::io_error);
    }
    resource_common::load_samples loads;
    loads.one_minute = samples[0];
    loads.five_minute = samples[1];
    loads.fifteen_minute = samples[2];
    return loads;
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
    constexpr int maximum_attempts = 4;
    constexpr std::size_t maximum_bytes = 256U * 1024U * 1024U;
    for (int attempt = 0; attempt < maximum_attempts; ++attempt) {
        std::size_t size = 0U;
        if (::sysctlbyname("kern.proc.proc", nullptr, &size, nullptr, 0U) !=
            0) {
            return fail(native_error());
        }
        if (size == 0U || size % sizeof(struct ::kinfo_proc) != 0U) {
            return fail(errc::malformed_data);
        }
        if (size > maximum_bytes) {
            return fail(errc::value_too_large);
        }

        const std::size_t initial_count = size / sizeof(struct ::kinfo_proc);
        if (initial_count >= (std::numeric_limits<std::size_t>::max)() - 16U) {
            return fail(errc::value_too_large);
        }
        std::vector<struct ::kinfo_proc> processes(initial_count + 16U);
        size = processes.size() * sizeof(struct ::kinfo_proc);
        if (::sysctlbyname("kern.proc.proc", processes.data(), &size, nullptr,
                           0U) != 0) {
            if (errno == ENOMEM) {
                continue;
            }
            return fail(native_error());
        }
        if (size == 0U || size % sizeof(struct ::kinfo_proc) != 0U ||
            size > processes.size() * sizeof(struct ::kinfo_proc)) {
            return fail(errc::malformed_data);
        }
        return static_cast<std::uint64_t>(size / sizeof(struct ::kinfo_proc));
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::uint64_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_file_count() {
    const result<std::int64_t> value = read_count_sysctl("kern.openfiles");
    if (!value) {
        return fail(value.error());
    }
    return static_cast<std::uint64_t>(*value);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    const result<std::int64_t> value = read_count_sysctl("kern.maxfiles");
    if (!value) {
        return fail(value.error());
    }
    return static_cast<std::uint64_t>(*value);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
