#ifndef SYSCAPE_ENVIRONMENT_HPP
#define SYSCAPE_ENVIRONMENT_HPP

/// @file
/// @brief Hosted environment variables, standard directories, and interactive terminal queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux and macOS use their documented POSIX and platform directory
/// facilities; Windows provides a native Win32/Shell known-folder backend.
/// Android, Apple mobile platforms, and other targets use the generic fallback.
/// @note All returned paths and strings are UTF-8 encoded.
/// @note Thread-safety: queries observe the process environment without
/// modifying it. C and POSIX environment mutation APIs do not provide a
/// portable synchronization contract with concurrent readers, so callers must
/// serialize calls that change the process environment against these queries.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/environment.hpp requires C++17 or later"
#endif

#include <string>
#include <string_view>

#include <syscape/detail/environment/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/environment/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/environment/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/environment/macos.hpp>
#else
#include <syscape/detail/environment/generic.hpp>
#endif

namespace syscape {
namespace environment {

/// Returns the value of the specified environment variable.
///
/// \param name The environment variable name. Must be non-empty, well-formed
/// UTF-8, and must not contain '=' or '\0'.
/// \return The UTF-8 string value of the environment variable, or an error:
/// - \c errc::invalid_argument if the name is empty or contains '=' or '\0'.
/// - \c errc::invalid_encoding if the name is not valid UTF-8.
/// - \c errc::not_found if the variable is not set.
/// - \c errc::not_supported if the platform does not support environment access.
inline result<std::string> get(std::string_view name) {
    return detail::environment_backend::get(name);
}

/// Checks whether the specified environment variable exists in the process environment.
///
/// \param name The environment variable name.
/// \return \c true if set, \c false if unset, or an error on invalid arguments.
inline result<bool> has(std::string_view name) {
    return detail::environment_backend::has(name);
}

/// Returns the platform-specific directory used for temporary files.
///
/// The returned path is normalized without a trailing directory separator
/// (except for root paths).
///
/// \return The UTF-8 directory path, or an error code.
inline result<std::string> temp_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::temp_directory());
}

/// Returns the user's home/profile directory.
///
/// \return The UTF-8 directory path, or an error code.
inline result<std::string> home_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::home_directory());
}

/// Returns the standard user configuration directory.
///
/// On Linux/POSIX: $XDG_CONFIG_HOME or $HOME/.config
/// On macOS: $HOME/Library/Application Support
/// On Windows: %APPDATA% (Roaming AppData)
///
/// \return The UTF-8 directory path, or an error code.
inline result<std::string> config_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::config_directory());
}

/// Returns the standard user persistent data directory.
///
/// On Linux/POSIX: $XDG_DATA_HOME or $HOME/.local/share
/// On macOS: $HOME/Library/Application Support
/// On Windows: %LOCALAPPDATA% (Local AppData)
///
/// \return The UTF-8 directory path, or an error code.
inline result<std::string> data_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::data_directory());
}

/// Returns the standard user cache directory.
///
/// On Linux/POSIX: $XDG_CACHE_HOME or $HOME/.cache
/// On macOS: $HOME/Library/Caches
/// On Windows: %LOCALAPPDATA% (Local AppData)
///
/// \return The UTF-8 directory path, or an error code.
inline result<std::string> cache_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::cache_directory());
}

/// Checks whether standard input (stdin) is attached to an interactive terminal.
///
/// \return \c true if stdin is an interactive terminal/console, \c false if redirected or closed.
inline result<bool> is_interactive_stdin() {
    return detail::environment_backend::is_interactive_stdin();
}

/// Checks whether standard output (stdout) is attached to an interactive terminal.
///
/// \return \c true if stdout is an interactive terminal/console, \c false if redirected to a pipe/file.
inline result<bool> is_interactive_stdout() {
    return detail::environment_backend::is_interactive_stdout();
}

/// Checks whether standard error (stderr) is attached to an interactive terminal.
///
/// \return \c true if stderr is an interactive terminal/console, \c false if redirected to a pipe/file.
inline result<bool> is_interactive_stderr() {
    return detail::environment_backend::is_interactive_stderr();
}

} // namespace environment
} // namespace syscape

#endif
