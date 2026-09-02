#ifndef SYSCAPE_DETAIL_RESOURCE_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_RESOURCE_APPLE_MOBILE_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <system_error>
#include <vector>

#include <sys/sysctl.h>
#include <sys/types.h>

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
        if (errno == EPERM || errno == EACCES) {
            return fail(errc::permission_denied);
        }
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
    int mib[3] = {CTL_KERN, KERN_PROC, KERN_PROC_ALL};
    constexpr int mib_length = 3;

    std::size_t estimate = 0U;
    if (::sysctl(mib, mib_length, nullptr, &estimate, nullptr, 0U) != 0) {
        if (errno == EPERM || errno == EACCES) {
            return fail(errc::permission_denied);
        }
        return fail(native_error());
    }
    if (estimate == 0U) {
        return fail(errc::not_found);
    }

    constexpr int growth_cap = 8;
    for (int attempt = 0; attempt < growth_cap; ++attempt) {
        if (estimate >
            (std::numeric_limits<std::size_t>::max)() - estimate / 8U - 64U) {
            return fail(errc::value_too_large);
        }
        std::vector<char> buffer(estimate + estimate / 8U + 64U);
        std::size_t size = buffer.size();
        if (::sysctl(mib, mib_length, buffer.data(), &size, nullptr, 0U) == 0) {
            if (size == 0U || size % sizeof(::kinfo_proc) != 0U) {
                return fail(errc::malformed_data);
            }
            return static_cast<std::uint64_t>(size / sizeof(::kinfo_proc));
        }
        if (errno == EPERM || errno == EACCES) {
            return fail(errc::permission_denied);
        }
        if (errno != ENOMEM) {
            return fail(native_error());
        }
        const std::size_t required = size;
        if (required > 0U) {
            estimate = required;
        } else {
            if (buffer.size() >
                (std::numeric_limits<std::size_t>::max)() / 2U) {
                return fail(errc::value_too_large);
            }
            estimate = buffer.size() * 2U;
        }
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::uint64_t> thread_count() {
    const result<std::int64_t> value = read_count_sysctl("kern.num_threads");
    if (!value) {
        return fail(value.error());
    }
    return static_cast<std::uint64_t>(*value);
}

inline result<std::uint64_t> open_file_count() {
    const result<std::int64_t> value = read_count_sysctl("kern.num_files");
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
