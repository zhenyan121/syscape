#ifndef SYSCAPE_DETAIL_RESOURCE_NETBSD_HPP
#define SYSCAPE_DETAIL_RESOURCE_NETBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>

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

inline result<std::int64_t> read_mib_int(const int* mib, unsigned int mib_len) {
    std::size_t size = 0U;
    int mib_copy[8];
    if (mib_len > 8U) {
        return fail(errc::not_supported);
    }
    for (unsigned int i = 0; i < mib_len; ++i) {
        mib_copy[i] = mib[i];
    }
    if (::sysctl(mib_copy, mib_len, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(native_error());
    }
    if (size == sizeof(std::int32_t)) {
        std::int32_t narrow = 0;
        std::size_t narrow_size = sizeof(narrow);
        if (::sysctl(mib_copy, mib_len, &narrow, &narrow_size, nullptr, 0U) !=
            0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
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
        if (::sysctl(mib_copy, mib_len, &wide, &wide_size, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
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
#if defined(KERN_NPROCS)
    int mib[] = {CTL_KERN, KERN_NPROCS};
    const result<std::int64_t> count = read_mib_int(mib, 2U);
    if (!count) {
        return fail(count.error());
    }
    return static_cast<std::uint64_t>(*count);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_file_count() {
#if defined(KERN_NFILES)
    int mib[] = {CTL_KERN, KERN_NFILES};
    const result<std::int64_t> count = read_mib_int(mib, 2U);
    if (!count) {
        return fail(count.error());
    }
    return static_cast<std::uint64_t>(*count);
#else
    return fail(errc::not_supported);
#endif
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    int mib[] = {CTL_KERN, KERN_MAXFILES};
    const result<std::int64_t> limit = read_mib_int(mib, 2U);
    if (!limit) {
        return fail(limit.error());
    }
    return static_cast<std::uint64_t>(*limit);
}

inline result<std::uint64_t> process_limit() {
    int mib[] = {CTL_KERN, KERN_MAXPROC};
    const result<std::int64_t> limit = read_mib_int(mib, 2U);
    if (!limit) {
        return fail(limit.error());
    }
    return static_cast<std::uint64_t>(*limit);
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
