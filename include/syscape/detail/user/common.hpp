#ifndef SYSCAPE_DETAIL_USER_COMMON_HPP
#define SYSCAPE_DETAIL_USER_COMMON_HPP

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_common {

/// Privilege classification of the calling process's effective identity.
///
/// The classification describes whether the identity used for permission
/// checks holds the platform's privileged account or an equivalent elevated
/// token. Finer-grained grants such as individual POSIX capabilities or
/// Windows per-privilege assignments are outside this classification; an
/// identity that holds only such grants reports unprivileged.
enum class privilege_state : std::uint8_t {
    /// The effective identity carries no special platform privilege.
    unprivileged,
    /// The effective identity is the platform's privileged account or an
    /// equivalently elevated token.
    privileged
};

/// Classification of a user login session type.
enum class session_type : std::uint8_t {
    /// The session type could not be determined.
    unknown,
    /// Direct physical console or text virtual terminal (e.g. tty1..tty6, console).
    console,
    /// Local graphical display session (e.g. X11 :0, Wayland, graphical desktop).
    graphical,
    /// Remote network login session (e.g. SSH, RDP, VNC).
    remote,
    /// Local pseudo-terminal window or terminal emulator (e.g. pts/0, pty).
    pseudo_terminal,
    /// Dedicated system or service session (e.g. Windows Session 0 / Services).
    system,
    /// Other or unclassified session type.
    other
};

/// Operational state of a login session.
enum class session_state : std::uint8_t {
    /// The session state could not be determined.
    unknown,
    /// The session is active and currently interacting or connected.
    active,
    /// The session is idle or resting.
    idle,
    /// The session is disconnected (e.g. disconnected Remote Desktop).
    disconnected,
    /// The session is locked, suspended, or in another transitional state.
    other
};

/// Information describing an active user login session.
struct session_info {
    /// Unique username recorded for this login session (UTF-8, non-empty).
    std::string user_name;

    /// Terminal device or station name (e.g. "pts/0", "tty1", "console", "rdp-tcp#0", ":0").
    std::string terminal;

    /// Platform-specific session identifier if exposed (e.g. "1" on Windows, ut_id on POSIX).
    std::optional<std::string> session_id;

    /// Process identifier (PID) of the session leader or login process, if recorded.
    std::optional<std::uint32_t> pid;

    /// Session type classification.
    session_type type = session_type::unknown;

    /// Session connection/activity state.
    session_state state = session_state::unknown;

    /// Remote client host name or IP address if this is a remote session.
    std::optional<std::string> remote_host;

    /// Instant when the session logged in or started.
    std::optional<std::chrono::system_clock::time_point> login_time;
};

/// Extracts unique, sorted user names from a sessions query result.
inline result<std::vector<std::string>> extract_logged_in_users(
    const result<std::vector<session_info>>& sessions_result) {
    if (!sessions_result) {
        return fail(sessions_result.error());
    }
    std::vector<std::string> users;
    users.reserve(sessions_result->size());
    for (const auto& session : *sessions_result) {
        if (!session.user_name.empty()) {
            users.push_back(session.user_name);
        }
    }
    std::sort(users.begin(), users.end());
    users.erase(std::unique(users.begin(), users.end()), users.end());
    return users;
}

/// Validates a user name reported by a platform backend.
///
/// A name must be non-empty and valid UTF-8. An empty name is malformed
/// platform data rather than valid information.
inline result<std::string> validate_utf8_name(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Validates a home-directory path reported by a platform backend.
///
/// The path must be non-empty and valid UTF-8. Platform backends enforce
/// their own absoluteness rules before the boundary validation.
inline result<std::string> validate_utf8_path(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Validates a login-shell value reported by a platform backend.
///
/// An empty shell is valid data where the platform records no shell, so only
/// the UTF-8 encoding is enforced.
inline result<std::string> validate_utf8_shell(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

} // namespace user_common
} // namespace detail
} // namespace syscape

#endif
