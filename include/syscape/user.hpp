#ifndef SYSCAPE_USER_HPP
#define SYSCAPE_USER_HPP

/// @file
/// @brief Hosted user identity and active login session queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - Real and effective user and group numeric identifiers (real_user_id(),
/// effective_user_id(), real_group_id(), effective_group_id()).
/// - Supplementary group numeric memberships (supplementary_groups()).
/// - Privilege state classification (privilege()).
/// - Login name, user name, home directory, and login shell (login_name(),
/// user_name(), home_directory(), shell()).
/// - Active user login sessions and logged-in user names (sessions(),
/// logged_in_users()).
/// @note Linux, macOS, and FreeBSD share a POSIX backend querying passwd,
/// groups, getlogin_r, and utmpx.
/// @note Windows provides a native backend querying GetUserNameW,
/// SHGetKnownFolderPath, process token elevation (Advapi32.lib), and Terminal
/// Services session enumeration (Wtsapi32.lib).
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.
/// @note Privacy: user names, home directories, and login sessions can identify
/// persons or accounts. Every query here is explicit, performs no logging,
/// persistence, telemetry, or network access, and preserves platform permission
/// errors.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/user.hpp requires C++17 or later"
#endif

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/user/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/user/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/user/netbsd.hpp>
#else
#include <syscape/detail/user/generic.hpp>
#endif

namespace syscape {
namespace user {

/// Privilege classification of the calling process's effective identity.
using privilege_state = detail::user_common::privilege_state;

/// Classification of a user login session type.
using session_type = detail::user_common::session_type;

/// Operational state of a login session.
using session_state = detail::user_common::session_state;

/// Information describing an active user login session.
using session_info = detail::user_common::session_info;

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

/// Returns the supplementary group identifiers of the calling process.
///
/// The set contains the operating-system-recorded group memberships of the
/// calling process at the time of the query, reported in ascending order
/// with duplicates removed so that comparisons are deterministic. The set is
/// a snapshot; concurrent membership changes become visible only to later
/// calls. An empty set is valid data meaning that the platform records no
/// supplementary membership beyond the effective group, and it is not an
/// error. Platforms record this set per process; whether it includes the
/// effective group identifier is platform-defined and is not normalized.
/// @return Ascending unique group identifiers, not_supported when the
/// platform defines no numeric supplementary-group concept, value_too_large
/// when a native identifier exceeds the portable width, or a native platform
/// error.
inline result<std::vector<std::uint32_t>> supplementary_groups() {
    return detail::user_backend::supplementary_groups();
}

/// Classifies the privilege of the effective identity used for permission
/// checks.
///
/// The classification reports whether that identity holds the platform's
/// privileged account or an equivalent elevated token: on POSIX platforms an
/// effective user identifier of zero classifies as privileged; on Windows
/// the documented token-elevation state of the process token classifies the
/// token. Finer-grained grants such as individual POSIX capabilities or
/// Windows per-privilege assignments are outside this classification and an
/// identity holding only such grants reports unprivileged. The state can
/// change where the platform permits identity change for the running
/// program.
/// @return A privilege classification, not_supported when the platform
/// exposes no acceptable source, or a native platform error such as an
/// access-denied token query failure.
inline result<privilege_state> privilege() {
    return detail::user_backend::privilege();
}

/// Returns the login name recorded by the platform for the calling process's
/// login session.
///
/// POSIX platforms report the name recorded in the session database entry of
/// the controlling terminal, which can differ from user_name() after tools
/// such as su or sudo changed the effective identity without changing the
/// login session. A process without a controlling terminal, or whose session
/// carries no recorded entry, has no login-session identity and receives
/// not_found rather than fabricated data. Each call performs a fresh lookup,
/// and the name is reported verbatim and is not normalized across platforms.
/// Session metadata beyond the recorded name, such as terminal devices,
/// timestamps, or remote hosts, is outside this slice.
/// @return A non-empty UTF-8 login name, not_found when the platform records
/// no login-session identity for this process, malformed_data for invalid
/// platform data, invalid_encoding when the native text is not valid UTF-8,
/// not_supported when the platform exposes no acceptable source, or a native
/// platform error.
inline result<std::string> login_name() {
    return detail::user_common::validate_utf8_name(
        detail::user_backend::login_name());
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

/// Returns all active user login sessions recorded by the operating system.
///
/// POSIX platforms query the system utmpx database in-process without spawning
/// external commands like who or w. Windows queries the Terminal Services
/// session API (WTSEnumerateSessionsW and WTSQuerySessionInformationW).
/// The returned list contains active user sessions sorted deterministically
/// by username, terminal, and session ID.
/// @return A list of active login sessions, not_supported when the platform
/// exposes no acceptable session source, or a native platform error.
inline result<std::vector<session_info>> sessions() {
    return detail::user_backend::sessions();
}

/// Returns the unique user names of all currently logged-in active users.
///
/// Derived from active user sessions with duplicates removed and sorted
/// alphabetically. If no users are logged in, returns an empty list.
/// @return Sorted unique user names, not_supported when the platform
/// exposes no acceptable session source, or a native platform error.
inline result<std::vector<std::string>> logged_in_users() {
    return detail::user_common::extract_logged_in_users(
        detail::user_backend::sessions());
}

} // namespace user
} // namespace syscape

#endif
