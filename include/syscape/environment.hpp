#ifndef SYSCAPE_ENVIRONMENT_HPP
#define SYSCAPE_ENVIRONMENT_HPP

/// @file
/// @brief Hosted environment variables, standard directories, and interactive
/// terminal queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, OpenHarmony, and
/// Emscripten).
/// @note Linux, macOS, FreeBSD, Solaris, Haiku, AIX, HP-UX, GNU/Hurd,
/// SerenityOS, and Redox OS use their documented POSIX and platform directory
/// facilities; Windows provides a native
/// Win32/Shell known-folder backend; Android provides environment variables and
/// working directory queries; Apple mobile platforms (iOS, iPadOS, tvOS,
/// watchOS, visionOS, and Mac Catalyst) provide POSIX environment variables,
/// temporary directory, and terminal queries under sandbox constraints. RTEMS
/// provides process environment and working-directory queries; temporary and
/// home directories are returned only when the recorded path exists, while
/// configuration, data, and cache directories report not_supported.
/// Emscripten provides process-environment snapshots, working-directory and
/// terminal queries, validates configured temporary and home directories, and
/// reports executable discovery as not_supported. Other targets use the
/// generic fallback.
/// @note All returned paths and strings are UTF-8 encoded.
/// @note Thread-safety: queries observe the process environment without
/// modifying it. C and POSIX environment mutation APIs do not provide a
/// portable synchronization contract with concurrent readers, so callers must
/// serialize calls that change the process environment against these queries.
/// @note Zephyr reports not_supported because no verified base configuration
/// provides these process-environment path and terminal semantics.

/// @note NuttX implements configuration-gated process environment queries,
/// working-directory and terminal queries, and validated temporary and home
/// directory lookups. Standard config, data, and cache directories report
/// not_supported.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/environment.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace syscape {
namespace environment {

/// One key-value pair in the process environment snapshot.
struct environment_variable {
    /// Variable name rendered as UTF-8. Guaranteed non-empty and contains no '=' or '\0'.
    std::string name;
    /// Variable value rendered as UTF-8.
    std::string value;
};

/// Equality comparison for environment variable entries.
inline bool operator==(const environment_variable& lhs, const environment_variable& rhs) noexcept {
    return lhs.name == rhs.name && lhs.value == rhs.value;
}

/// Inequality comparison for environment variable entries.
inline bool operator!=(const environment_variable& lhs, const environment_variable& rhs) noexcept {
    return !(lhs == rhs);
}

} // namespace environment
} // namespace syscape

#include <syscape/detail/environment/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX) &&           \
    !defined(SYSCAPE_TARGET_HURD) && !defined(SYSCAPE_TARGET_SERENITY) &&      \
    !defined(SYSCAPE_TARGET_REDOX) && !defined(SYSCAPE_TARGET_MINIX) &&        \
    !defined(SYSCAPE_TARGET_QNX) && !defined(SYSCAPE_TARGET_VXWORKS) &&        \
    !defined(SYSCAPE_TARGET_RTEMS) && !defined(SYSCAPE_TARGET_ZEPHYR) &&       \
    !defined(SYSCAPE_TARGET_NUTTX) && !defined(SYSCAPE_TARGET_WASI) &&         \
    !defined(SYSCAPE_TARGET_EMSCRIPTEN) && !defined(SYSCAPE_TARGET_FUCHSIA) && \
    !defined(SYSCAPE_TARGET_FREERTOS) && !defined(SYSCAPE_TARGET_THREADX) &&   \
    !defined(SYSCAPE_TARGET_EMBOS) && !defined(SYSCAPE_TARGET_UCOS) &&         \
    !defined(SYSCAPE_TARGET_INTEGRITY) && !defined(SYSCAPE_TARGET_TKERNEL)
#include <syscape/detail/environment/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/environment/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/environment/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/environment/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/environment/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/environment/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/environment/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/environment/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/environment/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/environment/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/environment/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/environment/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/environment/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/environment/hpux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HURD)
#include <syscape/detail/environment/hurd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_SERENITY)
#include <syscape/detail/environment/serenity.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_REDOX)
#include <syscape/detail/environment/redox.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MINIX)
#include <syscape/detail/environment/minix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_QNX)
#include <syscape/detail/environment/qnx.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_VXWORKS)
#include <syscape/detail/environment/vxworks.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_RTEMS)
#include <syscape/detail/environment/rtems.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_ZEPHYR)
#include <syscape/detail/environment/zephyr.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_NUTTX)
#include <syscape/detail/environment/nuttx.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_WASI)
#include <syscape/detail/environment/wasi.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_EMSCRIPTEN)
#include <syscape/detail/environment/emscripten.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_FUCHSIA)
#include <syscape/detail/environment/fuchsia.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_FREERTOS)
#include <syscape/detail/environment/freertos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_THREADX)
#include <syscape/detail/environment/threadx.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_EMBOS)
#include <syscape/detail/environment/embos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_UCOS)
#include <syscape/detail/environment/ucos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_INTEGRITY)
#include <syscape/detail/environment/integrity.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_TKERNEL)
#include <syscape/detail/environment/tkernel.hpp>
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

/// Captures a snapshot of all environment variables currently exported in the process.
///
/// The returned list is ordered lexicographically by variable name.
///
/// \return An ordered list of key-value environment variables, or an error:
/// - \c errc::invalid_encoding if an environment entry contains invalid UTF-8.
/// - \c errc::not_supported if the platform does not support environment enumeration.
inline result<std::vector<environment_variable>> environment_variables() {
    return detail::environment_backend::environment_variables();
}

/// Returns the current working directory of the calling process.
///
/// The returned path is normalized without a trailing directory separator
/// (except for root paths).
///
/// \return The UTF-8 normalized absolute directory path, or an error code.
inline result<std::string> current_working_directory() {
    return detail::environment_common::validate_utf8_path(
        detail::environment_backend::current_working_directory());
}

/// Searches for an executable by name within the directories listed in the PATH environment variable.
///
/// If \p name contains a directory separator, it is checked directly without searching PATH.
/// On Windows, standard executable extensions (.exe, .cmd, .bat, etc.) are automatically tested
/// if no extension is specified.
///
/// \param name The executable file name or command to find. Must be non-empty, valid UTF-8, and contain no '\0'.
/// \return The full normalized UTF-8 path to the executable, or \c errc::not_found if not found.
inline result<std::string> find_executable(std::string_view name) {
    const auto valid =
        detail::environment_common::validate_executable_name(name);
    if (!valid) {
        return fail(valid.error());
    }
    return detail::environment_backend::find_executable(name);
}

/// Returns the platform-specific path list delimiter character (':' on POSIX, ';' on Windows).
constexpr char path_list_separator() noexcept {
#if defined(_WIN32)
    return ';';
#else
    return ':';
#endif
}

/// Returns the primary platform-specific directory separator ('/' on POSIX, '\\' on Windows).
constexpr char directory_separator() noexcept {
#if defined(_WIN32)
    return '\\';
#else
    return '/';
#endif
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
