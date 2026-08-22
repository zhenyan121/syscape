#ifndef SYSCAPE_USER_HPP
#define SYSCAPE_USER_HPP

/// @file
/// @brief Hosted user identity queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux and macOS share a POSIX backend; Windows provides a native
/// backend for the textual queries. Other targets use the generic
/// not-supported fallback.
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.
/// @note Privacy: user names and home directories can identify persons or
/// accounts. Every query here is explicit, performs no logging, persistence,
/// telemetry, or network access, and preserves platform permission errors.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/user.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>

#include <syscape/detail/user/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/user/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/user/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/user/macos.hpp>
#else
#include <syscape/detail/user/generic.hpp>
#endif

namespace syscape {
namespace user {

/// Returns the real user identifier of the calling process.
///
/// The identifier is an operating-system-scoped number representing the
/// process's real user identity at the time of the query. It can change where
/// the platform permits process identity changes, can be shared or reused by
/// account records, and has no portable meaning across independent systems.
/// Zero is valid where the platform defines it (the privileged account on
/// POSIX systems); it is not an error sentinel. Platforms without this
/// concept return not_supported.
/// @return A user identifier, not_supported when the platform defines no such
/// concept, value_too_large when the native identifier exceeds the portable
/// width, or a native platform error.
inline result<std::uint32_t> real_user_id() {
    return detail::user_backend::real_user_id();
}

/// Returns the effective user identifier used for permission checks.
///
/// The effective identifier can differ from the real identifier where the
/// platform permits identity change for the running program, and it drives
/// the textual lookups performed by user_name(), home_directory(), and
/// shell(). See real_user_id() for identifier semantics.
/// @return A user identifier, not_supported when the platform defines no such
/// concept, value_too_large when the native identifier exceeds the portable
/// width, or a native platform error.
inline result<std::uint32_t> effective_user_id() {
    return detail::user_backend::effective_user_id();
}

/// Returns the real group identifier of the calling process.
///
/// Group identifiers follow the same scoping rules as user identifiers; see
/// real_user_id(). Supplementary group membership is outside this first
/// slice.
/// @return A group identifier, not_supported when the platform defines no
/// such concept, value_too_large when the native identifier exceeds the
/// portable width, or a native platform error.
inline result<std::uint32_t> real_group_id() {
    return detail::user_backend::real_group_id();
}

/// Returns the effective group identifier used for permission checks.
///
/// See real_group_id() for identifier semantics.
/// @return A group identifier, not_supported when the platform defines no
/// such concept, value_too_large when the native identifier exceeds the
/// portable width, or a native platform error.
inline result<std::uint32_t> effective_group_id() {
    return detail::user_backend::effective_group_id();
}

/// Returns the login name recorded by the platform for the effective user.
///
/// Each call performs a fresh lookup against the platform user database, so
/// concurrent changes to that database become visible between calls; the
/// returned value is a snapshot and no stability is guaranteed. The name is
/// reported verbatim and is not normalized across platforms.
/// @return A non-empty UTF-8 login name, not_found when the platform records
/// no entry for the effective user, malformed_data for invalid platform data,
/// invalid_encoding when the native text is not valid UTF-8, not_supported
/// when the platform exposes no acceptable source, or a native platform error
/// such as a directory-service failure.
inline result<std::string> user_name() {
    return detail::user_common::validate_utf8_name(
        detail::user_backend::user_name());
}

/// Returns the home directory recorded by the platform for the effective
/// user.
///
/// The path comes from the same lookup as user_name(), is reported verbatim
/// without canonicalization, and may refer to a location that was renamed,
/// unlinked, or is not currently accessible. Symbolic links are preserved.
/// The value can differ from any process environment variable because the
/// platform database is authoritative here.
/// @return An absolute UTF-8 directory path, not_found when the platform
/// records no entry for the effective user, malformed_data for invalid or
/// relative platform data, invalid_encoding when the native text is not valid
/// UTF-8, not_supported when the platform exposes no acceptable source, or a
/// native platform error such as a directory-service failure.
inline result<std::string> home_directory() {
    return detail::user_common::validate_utf8_path(
        detail::user_backend::home_directory());
}

/// Returns the login shell recorded by the platform for the effective user.
///
/// The shell is reported verbatim from the platform user database. An empty
/// value is valid data where the platform records no shell for the account;
/// it does not imply a default shell. The recorded program need not exist or
/// be executable at query time.
/// @return A UTF-8 shell value that may be empty, not_found when the platform
/// records no entry for the effective user, malformed_data for invalid
/// platform data, invalid_encoding when the native text is not valid UTF-8,
/// not_supported when the platform exposes no acceptable source, or a native
/// platform error such as a directory-service failure.
inline result<std::string> shell() {
    return detail::user_common::validate_utf8_shell(
        detail::user_backend::shell());
}

} // namespace user
} // namespace syscape

#endif
