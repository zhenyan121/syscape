#ifndef SYSCAPE_DETAIL_RESOURCE_HPUX_HPP
#define SYSCAPE_DETAIL_RESOURCE_HPUX_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <unistd.h>

#if defined(__has_include)
#if __has_include(<sys/pstat.h>)
#include <sys/pstat.h>
#define SYSCAPE_HAS_HPUX_PSTAT 1
#endif
#endif

#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline result<resource_common::load_samples> load_average() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_dynamic psd {};
    errno = 0;
    const int count = ::pstat_getdynamic(&psd, sizeof(psd), 1, 0);
    if (count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (count != 1) {
        return fail(errc::malformed_data);
    }
    if (psd.psd_avg_1_min < 0.0 || psd.psd_avg_5_min < 0.0 ||
        psd.psd_avg_15_min < 0.0) {
        return fail(errc::malformed_data);
    }
    resource_common::load_samples result {};
    result.one_minute = psd.psd_avg_1_min;
    result.five_minute = psd.psd_avg_5_min;
    result.fifteen_minute = psd.psd_avg_15_min;
    return result;
#else
    return fail(errc::not_supported);
#endif
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
#if defined(SYSCAPE_HAS_HPUX_PSTAT)
    struct pst_dynamic psd {};
    errno = 0;
    const int count = ::pstat_getdynamic(&psd, sizeof(psd), 1, 0);
    if (count < 0) {
        const int saved_errno = errno;
        if (saved_errno == EACCES || saved_errno == EPERM) {
            return fail(errc::permission_denied);
        }
        if (saved_errno != 0) {
            return fail(std::error_code(saved_errno, std::generic_category()));
        }
        return fail(errc::not_supported);
    }
    if (count == 0) {
        return fail(errc::temporarily_unavailable);
    }
    if (count != 1) {
        return fail(errc::malformed_data);
    }
    if (psd.psd_activeprocs < 0) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint64_t>(psd.psd_activeprocs);
#else
    return fail(errc::not_supported);
#endif
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
