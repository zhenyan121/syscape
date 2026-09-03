#ifndef SYSCAPE_SOFTWARE_HPP
#define SYSCAPE_SOFTWARE_HPP

/// @file
/// @brief Hosted system software, service, driver, update, package, and runtime
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile sandboxes expose no acceptable system-wide service,
/// driver, package, update, or runtime inventory source to this interface, so
/// all queries report not_supported.
/// @note This module exposes:
/// - Enumeration of system services and daemons (services()).
/// - Lookup of a specific system service by name (find_service(name)).
/// - Enumeration of loaded kernel drivers and modules (loaded_drivers()).
/// - Lookup of a specific loaded kernel driver by name (find_driver(name)).
/// - Enumeration of installed software packages and applications
/// (installed_packages()).
/// - Lookup of an installed software package by name (find_package(name)).
/// - Enumeration of observable system update state (system_updates()).
/// - Enumeration of installed execution and development runtimes
/// (installed_runtimes()).
/// @note Linux parses systemd unit files, /proc/modules, pacman/dpkg/apk
/// databases, and freedesktop application entries in-process without spawning
/// subprocesses.
/// @note Windows queries the Service Control Manager (SCM), Psapi driver APIs,
/// Uninstall and runtime registry catalogs, and CBS update state
/// (requires linking Advapi32.lib and Psapi.lib).
/// @note macOS parses LaunchDaemons, LaunchAgents, and .app bundles using
/// CoreFoundation together with update metadata and runtime installation files
/// (requires linking -framework CoreFoundation). loaded_drivers() returns
/// not_supported on macOS as Darwin provides no unprivileged in-process public
/// API for loaded kernel modules.
/// @note Software and service states change dynamically. Queries query on
/// demand without caching.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/software.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <syscape/detail/software/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/software/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/software/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/software/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/software/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/software/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/software/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/software/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/software/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/software/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/software/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/software/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/software/haiku.hpp>
#else
#include <syscape/detail/software/generic.hpp>
#endif

namespace syscape {
namespace software {

/// Operational lifecycle state of a system service or daemon.
using service_state = detail::software_common::service_state;

/// Configured startup policy of a system service.
using service_startup = detail::software_common::service_startup;

/// Operational lifecycle state of a kernel driver or module.
using driver_state = detail::software_common::driver_state;

/// Packaging or distribution format of an installed software package or application.
using package_format = detail::software_common::package_format;

/// Classification of a system software update.
using update_classification = detail::software_common::update_classification;

/// Assessed severity level of a system software or security update.
using update_severity = detail::software_common::update_severity;

/// Classification of a discovered runtime or language toolchain.
using runtime_kind = detail::software_common::runtime_kind;

/// Observable metadata describing an OS service or background daemon.
struct service_entry {
    /// Unique service identifier (e.g. "sshd.service", "Spooler", "com.apple.syslogd").
    std::string name;

    /// Human-readable display name, or no value if not exposed.
    std::optional<std::string> display_name;

    /// Detailed description of the service's purpose.
    std::optional<std::string> description;

    /// Operational state (running, stopped, paused, etc.).
    service_state state = service_state::unknown;

    /// Configured startup type (automatic, manual, disabled).
    service_startup startup_type = service_startup::unknown;

    /// Process ID (PID) if the service is currently running and observable.
    std::optional<std::uint32_t> pid;

    /// Executable binary path or command line.
    std::optional<std::string> executable_path;
};

/// Observable metadata describing a loaded kernel driver or module.
struct driver_entry {
    /// Module or driver identifier (e.g. "ext4", "ntfs.sys").
    std::string name;

    /// In-memory size in bytes occupied by the driver.
    std::optional<std::uint64_t> size_bytes;

    /// Usage / reference count of instances depending on the driver.
    std::optional<std::uint32_t> use_count;

    /// Current operational state (loaded, running, unloading).
    driver_state state = driver_state::unknown;

    /// File path on disk if reported by the operating system.
    std::optional<std::string> path;
};

/// Observable metadata describing an installed software package or application.
struct package_entry {
    /// Package or application name (e.g. "git", "Visual Studio Code").
    std::string name;

    /// Version identifier (e.g. "2.44.0").
    std::optional<std::string> version;

    /// Software publisher, vendor, or maintainer.
    std::optional<std::string> publisher;

    /// Filesystem installation root or bundle location.
    std::optional<std::string> install_location;

    /// Target architecture (e.g. "x86_64", "amd64").
    std::optional<std::string> architecture;

    /// Packaging format / catalog source.
    package_format format = package_format::unknown;

    /// Brief description of the package.
    std::optional<std::string> description;
};

/// Observable metadata describing an OS-level software or security update.
struct update_entry {
    /// Unique update identifier (e.g., "KB5034441", "linux-6.10.3", "system-reboot-required").
    std::string identifier;

    /// Human-readable update title or package name.
    std::string title;

    /// Target or available version string.
    std::optional<std::string> version;

    /// Functional classification of the update (security, bugfix, feature, definition, etc.).
    update_classification classification = update_classification::unknown;

    /// Assessed severity level of the update.
    update_severity severity = update_severity::unknown;

    /// Whether applying this update is flagged by the OS as requiring a reboot.
    bool requires_reboot = false;

    /// Extended description or bulletin summary.
    std::optional<std::string> description;
};

/// Observable metadata describing an installed development or execution runtime.
struct runtime_entry {
    /// Runtime kind classification (.NET, Java, Python, Node, Rust, Go, etc.).
    runtime_kind kind = runtime_kind::unknown;

    /// Runtime display name (e.g. ".NET Runtime", "OpenJDK", "Python", "Go").
    std::string name;

    /// Version identifier (e.g. "8.0.7", "21.0.12.1", "3.12").
    std::string version;

    /// Installation root directory or primary binary path.
    std::string installation_path;

    /// Target architecture if exposed (e.g. "x86_64", "arm64").
    std::optional<std::string> architecture;
};

} // namespace software

namespace detail {
namespace software_public {

inline software::service_entry make_public_service(software_common::service_record&& rec) {
    software::service_entry entry;
    entry.name = std::move(rec.name);
    entry.display_name = std::move(rec.display_name);
    entry.description = std::move(rec.description);
    entry.state = rec.state;
    entry.startup_type = rec.startup_type;
    entry.pid = rec.pid;
    entry.executable_path = std::move(rec.executable_path);
    return entry;
}

inline software::driver_entry make_public_driver(software_common::driver_record&& rec) {
    software::driver_entry entry;
    entry.name = std::move(rec.name);
    entry.size_bytes = rec.size_bytes;
    entry.use_count = rec.use_count;
    entry.state = rec.state;
    entry.path = std::move(rec.path);
    return entry;
}

inline software::package_entry make_public_package(software_common::package_record&& rec) {
    software::package_entry entry;
    entry.name = std::move(rec.name);
    entry.version = std::move(rec.version);
    entry.publisher = std::move(rec.publisher);
    entry.install_location = std::move(rec.install_location);
    entry.architecture = std::move(rec.architecture);
    entry.format = rec.format;
    entry.description = std::move(rec.description);
    return entry;
}

inline result<software::update_entry> make_public_update(software_common::update_record&& rec) {
    if (!detail::is_valid_utf8(rec.identifier) ||
        !detail::is_valid_utf8(rec.title) ||
        (rec.version && !detail::is_valid_utf8(*rec.version)) ||
        (rec.description && !detail::is_valid_utf8(*rec.description))) {
        return fail(errc::invalid_encoding);
    }
    software::update_entry entry;
    entry.identifier = std::move(rec.identifier);
    entry.title = std::move(rec.title);
    entry.version = std::move(rec.version);
    entry.classification = rec.classification;
    entry.severity = rec.severity;
    entry.requires_reboot = rec.requires_reboot;
    entry.description = std::move(rec.description);
    return entry;
}

inline result<software::runtime_entry> make_public_runtime(software_common::runtime_record&& rec) {
    if (!detail::is_valid_utf8(rec.name) ||
        !detail::is_valid_utf8(rec.version) ||
        !detail::is_valid_utf8(rec.installation_path) ||
        (rec.architecture && !detail::is_valid_utf8(*rec.architecture))) {
        return fail(errc::invalid_encoding);
    }
    software::runtime_entry entry;
    entry.kind = rec.kind;
    entry.name = std::move(rec.name);
    entry.version = std::move(rec.version);
    entry.installation_path = std::move(rec.installation_path);
    entry.architecture = std::move(rec.architecture);
    return entry;
}

} // namespace software_public
} // namespace detail

namespace software {

/// Queries all visible system services and daemons.
/// @return Vector of service_entry, or an error if query fails.
inline result<std::vector<service_entry>> services() {
    auto records = detail::software_backend::services();
    if (!records) {
        return fail(records.error());
    }
    std::vector<service_entry> result_list;
    result_list.reserve(records->size());
    for (auto& rec : *records) {
        result_list.push_back(detail::software_public::make_public_service(std::move(rec)));
    }
    return result_list;
}

/// Finds a specific system service by its unique name or unit identifier.
/// @param name The service name to search for.
/// @return Matching service_entry, or errc::not_found if not located.
inline result<service_entry> find_service(std::string_view name) {
    auto rec = detail::software_backend::find_service(name);
    if (!rec) {
        return fail(rec.error());
    }
    return detail::software_public::make_public_service(std::move(*rec));
}

/// Queries all loaded kernel drivers and modules.
/// @return Vector of driver_entry, or an error if query fails.
inline result<std::vector<driver_entry>> loaded_drivers() {
    auto records = detail::software_backend::loaded_drivers();
    if (!records) {
        return fail(records.error());
    }
    std::vector<driver_entry> result_list;
    result_list.reserve(records->size());
    for (auto& rec : *records) {
        result_list.push_back(detail::software_public::make_public_driver(std::move(rec)));
    }
    return result_list;
}

/// Finds a specific loaded kernel driver by its module name.
/// @param name The module name to search for.
/// @return Matching driver_entry, or errc::not_found if not located.
inline result<driver_entry> find_driver(std::string_view name) {
    auto rec = detail::software_backend::find_driver(name);
    if (!rec) {
        return fail(rec.error());
    }
    return detail::software_public::make_public_driver(std::move(*rec));
}

/// Queries installed software packages and desktop applications.
/// @return Vector of package_entry, or an error if query fails.
inline result<std::vector<package_entry>> installed_packages() {
    auto records = detail::software_backend::installed_packages();
    if (!records) {
        return fail(records.error());
    }
    std::vector<package_entry> result_list;
    result_list.reserve(records->size());
    for (auto& rec : *records) {
        result_list.push_back(detail::software_public::make_public_package(std::move(rec)));
    }
    return result_list;
}

/// Finds a specific installed package or application by name.
/// @param name The package name to search for.
/// @return Matching package_entry, or errc::not_found if not located.
inline result<package_entry> find_package(std::string_view name) {
    auto rec = detail::software_backend::find_package(name);
    if (!rec) {
        return fail(rec.error());
    }
    return detail::software_public::make_public_package(std::move(*rec));
}

/// Queries observable system software update state.
///
/// Results may include updates pending application and already-applied packages
/// for which the platform reports that a reboot is still required. The returned
/// state can change while the process is running. Platform backends return
/// `errc::not_supported` when no documented source is available and preserve
/// permission, malformed-data, encoding, and native system errors. On Arch
/// Linux, this query runs `/usr/bin/checkupdates --nocolor`; it synchronizes an
/// isolated pacman database and can require network access. Exit status 2 is a
/// successful empty result, while synchronization failure reports
/// `errc::temporarily_unavailable`.
/// @return Observable update entries, or an error if the query cannot be
/// completed.
inline result<std::vector<update_entry>> system_updates() {
    auto records = detail::software_backend::system_updates();
    if (!records) {
        return fail(records.error());
    }
    std::vector<update_entry> result_list;
    result_list.reserve(records->size());
    for (auto& rec : *records) {
        auto entry = detail::software_public::make_public_update(std::move(rec));
        if (!entry) {
            return fail(entry.error());
        }
        result_list.push_back(std::move(*entry));
    }
    return result_list;
}

/// Queries installed language and execution runtimes (e.g. .NET, Java, Python, Node, Rust, Go).
///
/// The inventory is collected on demand and can change while the process is
/// running. A backend may report only runtimes exposed by documented system or
/// installation metadata. Permission, malformed-data, encoding, and native
/// system failures are returned rather than converted to a partial inventory.
/// @return Installed runtime entries, or an error if the query cannot be completed.
inline result<std::vector<runtime_entry>> installed_runtimes() {
    auto records = detail::software_backend::installed_runtimes();
    if (!records) {
        return fail(records.error());
    }
    std::vector<runtime_entry> result_list;
    result_list.reserve(records->size());
    for (auto& rec : *records) {
        auto entry = detail::software_public::make_public_runtime(std::move(rec));
        if (!entry) {
            return fail(entry.error());
        }
        result_list.push_back(std::move(*entry));
    }
    return result_list;
}

} // namespace software
} // namespace syscape

#endif
