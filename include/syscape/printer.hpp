#ifndef SYSCAPE_PRINTER_HPP
#define SYSCAPE_PRINTER_HPP

/// @file
/// @brief Hosted printer enumeration, queue status, connection, and capability
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - Enumeration of installed and discoverable printers (printers()).
/// - Total installed printer count (printer_count()).
/// - System default printer query (default_printer()).
/// - Lookup of an installed printer by queue name or display name (find_printer()).
/// - Operational state (idle, processing, stopped).
/// - Connection and device classification (local, network, virtual).
/// - Queue status (accepting jobs, shared state, queued job count).
/// - Reported hardware and driver capabilities (color, duplex, paper sizes,
/// resolutions, copy limits).
/// @note Linux and macOS query the local CUPS print system via its native Unix
/// domain socket using RFC 8010/8011 IPP messages. Linux can supplement CUPS
/// queues with direct USB printer nodes; both platforms can fall back to the
/// persistent CUPS configuration when no daemon is available.
/// @note Windows queries the official Win32 Print Spooler APIs (winspool.h).
/// @note Printing environments and queues can change dynamically. Queries do
/// not cache results.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/printer.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace syscape {
namespace printer {

/// Operational and queue state of a printer.
enum class printer_state : std::uint8_t {
    /// State is unknown or could not be determined.
    unknown,
    /// Printer is idle and ready to receive or process jobs.
    idle,
    /// Printer is actively processing or printing a job.
    processing,
    /// Printer is stopped, paused, or held in an error condition.
    stopped
};

/// Connection attachment and device classification of a printer.
enum class printer_type : std::uint8_t {
    /// Connection type is unknown or unclassified.
    unknown,
    /// Direct local hardware connection (e.g. USB, IEEE 1284 parallel).
    local,
    /// Network-attached printer (e.g. IPP, LPR/LPD, AppSocket/JetDirect, WSD).
    network,
    /// Virtual software print destination (e.g. Print to PDF, XPS, Fax).
    virtual_printer
};

/// Basic printer hardware and driver capabilities.
struct printer_capabilities {
    /// Whether color printing is supported.
    std::optional<bool> color;

    /// Whether two-sided (duplex) printing is supported.
    std::optional<bool> duplex;

    /// Whether multiple copies can be produced by hardware/driver.
    std::optional<bool> copies;

    /// Whether output collation is supported.
    std::optional<bool> collate;

    /// List of supported media or paper size identifiers (e.g. "iso_a4_210x297mm", "na_letter_8.5x11in", "A4").
    std::vector<std::string> supported_media;

    /// List of supported print resolutions (e.g. "600x600 dpi", "1200x1200 dpi").
    std::vector<std::string> supported_resolutions;

    /// Maximum number of copies supported in a single job if reported.
    std::optional<std::uint32_t> max_copies;
};

/// Information describing an installed or discoverable printer.
struct printer_info {
    /// Unique identifier or queue name (e.g. "HP_OfficeJet_Pro_9010", "Microsoft Print to PDF").
    std::string id;

    /// Human-friendly printer display name.
    std::string name;

    /// Driver, model, or make-and-model description.
    std::string driver_name;

    /// Physical location string if configured.
    std::string location;

    /// Description, comment, or info text.
    std::string description;

    /// Device URI or port name (e.g. "socket://192.168.1.50", "usb://HP/...", "PORTPROMPT:").
    std::string uri;

    /// Classification of printer connection (local, network, virtual).
    printer_type type = printer_type::unknown;

    /// Current operational state of the printer.
    printer_state state = printer_state::unknown;

    /// Whether this printer is marked as the system default, when the
    /// platform snapshot could determine that state.
    std::optional<bool> is_default;

    /// Whether this printer is shared across the network, when reported.
    std::optional<bool> is_shared;

    /// Whether this printer is currently accepting new print jobs, when
    /// reported by an authoritative platform source.
    std::optional<bool> is_accepting_jobs;

    /// Number of print jobs currently queued on this printer.
    std::optional<std::uint32_t> queued_job_count;

    /// Reported printer capabilities.
    printer_capabilities capabilities;
};

} // namespace printer
} // namespace syscape

#include <syscape/detail/printer/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__)
#include <syscape/detail/printer/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/printer/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__)
#include <syscape/detail/printer/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/printer/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/printer/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/printer/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/printer/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/printer/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/printer/solaris.hpp>
#else
#include <syscape/detail/printer/generic.hpp>
#endif

namespace syscape {
namespace printer {

/// Enumerates all installed and discoverable printers on the system.
///
/// @note Linux and macOS query CUPS via local Unix domain socket / IPP and
/// /etc/cups/printers.conf; Windows queries Win32 Spooler APIs; other platforms
/// return not_supported.
/// @return A vector of printer_info entries; not_supported when printing is
/// unavailable; permission_denied when access is denied; invalid_encoding or
/// malformed_data for unusable platform data; temporarily_unavailable for a
/// changing or timed-out snapshot; or a native I/O error.
inline result<std::vector<printer_info>> printers() {
    return detail::printer_backend::printers();
}

/// Returns the count of installed printers on the system.
///
/// @note The count is a fresh snapshot and can change immediately after return.
/// @return Printer count on success; or the enumeration error unchanged.
inline result<std::size_t> printer_count() {
    return detail::printer_backend::printer_count();
}

/// Returns the system default printer.
///
/// @return The default printer_info entry; not_found if no default printer is
/// configured; not_supported if the platform does not expose a default printer;
/// temporarily_unavailable if the default changes during the query; or an
/// error code describing the lookup failure.
inline result<printer_info> default_printer() {
    return detail::printer_backend::default_printer();
}

/// Finds an installed printer by queue name or display name.
///
/// @param name_or_id The identifier or human-readable name of the printer to
/// locate.
/// @return The matching printer_info entry; not_found if no matching printer
/// exists; or an error code.
inline result<printer_info> find_printer(std::string_view name_or_id) {
    return detail::printer_backend::find_printer(name_or_id);
}

} // namespace printer
} // namespace syscape

#endif // SYSCAPE_PRINTER_HPP
