#ifndef SYSCAPE_DETAIL_PROCESS_POSIX_HPP
#define SYSCAPE_DETAIL_PROCESS_POSIX_HPP

#include <cerrno>
#include <cstdint>
#include <system_error>
#include <type_traits>

#include <sys/resource.h>

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_posix {

/// Validates a nice value against the range documented by one POSIX target.
inline result<int> validate_priority(int value, int least_value,
                                     int greatest_value) {
    if (value < least_value || value > greatest_value) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Reads and validates the nice value recorded for the caller.
inline result<int> priority(int least_value, int greatest_value) {
    errno = 0;
    const int value = ::getpriority(::PRIO_PROCESS, 0);
    if (value == -1 && errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return validate_priority(value, least_value, greatest_value);
}

/// Converts one recorded resource-limit bound, honoring the platform's
/// no-bound marker before validating signed finite values.
template <typename Rlim>
inline result<process_common::resource_limit_bound> convert_rlim_amount(
    Rlim value, Rlim infinity_marker) {
    process_common::resource_limit_bound bound;
    if (value == infinity_marker) {
        bound.unlimited = true;
        return bound;
    }
    if constexpr (std::is_signed<Rlim>::value) {
        if (value < 0) { return fail(errc::malformed_data); }
    }
    bound.amount = static_cast<std::uint64_t>(value);
    return bound;
}

/// Enforces the POSIX invariant that the soft bound never exceeds the hard
/// bound, treating unlimited as greater than every finite amount.
inline result<void> validate_limit_pair(
    const process_common::resource_limit_bound& soft,
    const process_common::resource_limit_bound& hard) {
    const bool soft_within_hard =
        hard.unlimited ||
        (!soft.unlimited && soft.amount <= hard.amount);
    if (!soft_within_hard) { return fail(errc::malformed_data); }
    return {};
}

/// Maps one portable limit kind onto its documented getrlimit constant.
inline result<int> native_limit_resource(
    process_common::limit_resource kind) {
    switch (kind) {
        case process_common::limit_resource::core_file_size:
            return RLIMIT_CORE;
        case process_common::limit_resource::cpu_time:
            return RLIMIT_CPU;
        case process_common::limit_resource::file_size:
            return RLIMIT_FSIZE;
        case process_common::limit_resource::open_files:
            return RLIMIT_NOFILE;
        case process_common::limit_resource::stack_size:
            return RLIMIT_STACK;
        case process_common::limit_resource::address_space:
            return RLIMIT_AS;
    }
    return fail(errc::invalid_argument);
}

inline result<process_common::resource_limit_snapshot> resource_limit(
    process_common::limit_resource kind) {
    const result<int> native_kind = native_limit_resource(kind);
    if (!native_kind) { return fail(native_kind.error()); }

    struct ::rlimit limits {};
    errno = 0;
    if (::getrlimit(*native_kind, &limits) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const result<process_common::resource_limit_bound> soft =
        convert_rlim_amount(limits.rlim_cur, RLIM_INFINITY);
    if (!soft) { return fail(soft.error()); }
    const result<process_common::resource_limit_bound> hard =
        convert_rlim_amount(limits.rlim_max, RLIM_INFINITY);
    if (!hard) { return fail(hard.error()); }
    const result<void> ordered = validate_limit_pair(*soft, *hard);
    if (!ordered) { return fail(ordered.error()); }

    process_common::resource_limit_snapshot snapshot;
    snapshot.soft = *soft;
    snapshot.hard = *hard;
    return snapshot;
}

} // namespace process_posix
} // namespace detail
} // namespace syscape

#endif
