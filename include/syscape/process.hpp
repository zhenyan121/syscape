#ifndef SYSCAPE_PROCESS_HPP
#define SYSCAPE_PROCESS_HPP

/// @file
/// @brief Hosted process identity and execution-context queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux, Windows, and macOS have native backends. Other targets use
/// the generic not-supported fallback.
/// @note Expected failures are returned as native error codes where available,
/// or as syscape::errc values for missing, malformed, or unsupported data.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/process.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/process/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/process/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/process/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/process/macos.hpp>
#else
#include <syscape/detail/process/generic.hpp>
#endif

namespace syscape {
namespace process {

/// Returns the operating-system identifier of the calling process.
///
/// The identifier is unique among live processes for the current operating-
/// system instance. An identifier can be reused only after its process ends,
/// and identifiers have no meaning across independent machines or boots.
/// @return A positive process identifier or a platform error.
inline result<std::uint32_t> process_id() {
    return detail::process_backend::process_id();
}

/// Returns the operating-system identifier of the calling process's parent.
///
/// The value is a snapshot taken by the query or the underlying process
/// metadata at the moment of the call. A returned zero is valid where the
/// operating system defines it to mean that the process has no live parent;
/// it is not an error sentinel. After the parent exits, the platform may
/// report a reaper or another parent-process convention instead of the
/// original creator.
/// @return A process identifier, not_supported when the platform exposes no
/// acceptable source, not_found when no parent entry exists, malformed_data
/// for invalid platform data, or a native platform error.
inline result<std::uint32_t> parent_process_id() {
    return detail::process_backend::parent_process_id();
}

/// Returns the absolute filesystem path used to start the current program.
///
/// The path is reported verbatim from the platform and is not canonicalized:
/// symbolic links, relative components, and platform annotations are
/// preserved.
/// @return The executable path as UTF-8, not_supported when the platform
/// exposes no acceptable source, malformed_data for a relative or invalid
/// result, invalid_encoding for non-UTF-8 native text, or a native platform
/// error. A previously existing file may have been renamed, unlinked, or
/// replaced after process creation.
inline result<std::string> executable_path() {
    return detail::process_common::validate_utf8_path(
        detail::process_backend::executable_path());
}

/// Returns the argument values supplied to the current program.
///
/// The first element corresponds to the platform's argv[0] where that concept
/// exists and may therefore describe the program rather than be an argument
/// passed after it. Empty argument values are valid and are preserved. The
/// collection reflects process-start arguments except where a documented
/// platform interface explicitly exposes later modification. Platform sources
/// impose size limits on the argument data; an oversized source fails with
/// value_too_large.
/// @return Zero or more UTF-8 argument values. Most executions contain at
/// least argv[0], while an empty collection is valid where the platform
/// permits execution with no argument values. Returns not_found when the
/// platform has no acceptable command-line source, malformed_data for invalid
/// framing, invalid_encoding for non-UTF-8 text, not_supported when no such
/// interface exists, or a native platform error.
inline result<std::vector<std::string>> command_line() {
    return detail::process_common::validate_utf8_arguments(
        detail::process_backend::command_line());
}

/// Returns the absolute pathname of the calling process's working directory.
///
/// The value describes the directory used to resolve relative pathnames and
/// can change whenever the process successfully changes its working directory.
/// @return The working-directory path as UTF-8, not_supported when the
/// platform exposes no acceptable source, malformed_data for a relative or
/// invalid result, invalid_encoding for non-UTF-8 native text, or a native
/// platform error such as permission denied.
inline result<std::string> working_directory() {
    return detail::process_common::validate_utf8_path(
        detail::process_backend::working_directory());
}

} // namespace process
} // namespace syscape

#endif
