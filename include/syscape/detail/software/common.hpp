#ifndef SYSCAPE_DETAIL_SOFTWARE_COMMON_HPP
#define SYSCAPE_DETAIL_SOFTWARE_COMMON_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/config.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_common {

/// Operational state of a system service or daemon.
enum class service_state : std::uint8_t {
    unknown = 0,
    running = 1,
    stopped = 2,
    paused = 3,
    starting = 4,
    stopping = 5
};

/// Configured startup policy of a system service.
enum class service_startup : std::uint8_t {
    unknown = 0,
    automatic = 1,
    manual = 2,
    disabled = 3,
    delayed_automatic = 4
};

/// Operational state of a loaded kernel driver or module.
enum class driver_state : std::uint8_t {
    /// The platform did not expose a recognized operational state.
    unknown = 0,
    /// The driver is resident, but the platform did not report whether it is active.
    loaded = 1,
    /// The driver or module is active (for example, Linux reports it as Live).
    running = 2,
    /// The driver or module is being removed.
    unloading = 3
};

/// Packaging or distribution format of an installed software package or app.
enum class package_format : std::uint8_t {
    unknown = 0,
    dpkg = 1,
    rpm = 2,
    pacman = 3,
    apk = 4,
    desktop_entry = 5,
    windows_installer = 6,
    macos_bundle = 7
};

/// Classification of a system software update.
enum class update_classification : std::uint8_t {
    unknown = 0,        ///< The source did not expose a trustworthy classification
    security = 1,       ///< Security update or patch addressing vulnerabilities
    critical = 2,       ///< Critical non-security fix
    bugfix = 3,         ///< General bug fix or reliability update
    feature = 4,        ///< Feature or enhancement update
    definition = 5,     ///< Antivirus/malware definition or signature update
    driver = 6,         ///< Hardware driver or firmware update
    rollup = 7          ///< Cumulative update package or update rollup
};

/// Severity level of a system software or security update.
enum class update_severity : std::uint8_t {
    unknown = 0,       ///< The source did not expose a trustworthy severity
    unspecified = 1,   ///< The source explicitly reports no assigned severity
    low = 2,           ///< Low severity
    moderate = 3,      ///< Moderate severity
    important = 4,     ///< Important severity
    critical = 5       ///< Critical severity
};

/// Classification of a discovered runtime or language toolchain.
enum class runtime_kind : std::uint8_t {
    unknown = 0,      ///< Unrecognized runtime kind
    dotnet = 1,       ///< Microsoft .NET Runtime / SDK
    java = 2,         ///< Java Runtime Environment (JRE) / JDK
    python = 3,       ///< Python Interpreter
    nodejs = 4,       ///< Node.js Runtime
    rust = 5,         ///< Rust Toolchain (rustc/cargo)
    golang = 6,       ///< Go Runtime / Toolchain
    ruby = 7,         ///< Ruby Interpreter
    php = 8           ///< PHP Runtime
};

/// Internal record describing an OS service or daemon.
struct service_record {
    std::string name;
    std::optional<std::string> display_name;
    std::optional<std::string> description;
    service_state state = service_state::unknown;
    service_startup startup_type = service_startup::unknown;
    std::optional<std::uint32_t> pid;
    std::optional<std::string> executable_path;
};

/// Internal record describing a kernel driver or module.
struct driver_record {
    std::string name;
    std::optional<std::uint64_t> size_bytes;
    std::optional<std::uint32_t> use_count;
    driver_state state = driver_state::unknown;
    std::optional<std::string> path;
};

/// Internal record describing an installed package or application.
struct package_record {
    std::string name;
    std::optional<std::string> version;
    std::optional<std::string> publisher;
    std::optional<std::string> install_location;
    std::optional<std::string> architecture;
    package_format format = package_format::unknown;
    std::optional<std::string> description;
};

/// Internal record describing an OS software or security update.
struct update_record {
    std::string identifier;
    std::string title;
    std::optional<std::string> version;
    update_classification classification = update_classification::unknown;
    update_severity severity = update_severity::unknown;
    bool requires_reboot = false;
    std::optional<std::string> description;
};

/// Internal record describing an installed language or execution runtime.
struct runtime_record {
    runtime_kind kind = runtime_kind::unknown;
    std::string name;
    std::string version;
    std::string installation_path;
    std::optional<std::string> architecture;
};

/// Deterministic ordering for services.
inline bool compare_services(const service_record& lhs, const service_record& rhs) noexcept {
    return lhs.name < rhs.name;
}

/// Deterministic ordering for drivers.
inline bool compare_drivers(const driver_record& lhs, const driver_record& rhs) noexcept {
    return lhs.name < rhs.name;
}

/// Deterministic ordering for packages.
inline bool compare_packages(const package_record& lhs, const package_record& rhs) noexcept {
    if (lhs.name != rhs.name) {
        return lhs.name < rhs.name;
    }
    if (lhs.version != rhs.version) {
        return lhs.version < rhs.version;
    }
    return lhs.architecture < rhs.architecture;
}

/// Deterministic ordering for updates.
inline bool compare_updates(const update_record& lhs, const update_record& rhs) noexcept {
    if (lhs.identifier != rhs.identifier) {
        return lhs.identifier < rhs.identifier;
    }
    return lhs.title < rhs.title;
}

/// Deterministic ordering for runtimes.
inline bool compare_runtimes(const runtime_record& lhs, const runtime_record& rhs) noexcept {
    if (lhs.kind != rhs.kind) {
        return static_cast<int>(lhs.kind) < static_cast<int>(rhs.kind);
    }
    if (lhs.name != rhs.name) {
        return lhs.name < rhs.name;
    }
    if (lhs.version != rhs.version) {
        return lhs.version < rhs.version;
    }
    return lhs.installation_path < rhs.installation_path;
}

/// Helper to append length-prefixed field to avoid key collision.
inline void append_length_prefixed(std::string& out, std::string_view val) {
    out.append(std::to_string(val.size()));
    out.push_back(':');
    out.append(val);
}

/// Constructs a strictly collision-free deduplication key for package records using length-prefix encoding.
inline std::string make_package_dedup_key(const package_record& rec) {
    std::string key;
    append_length_prefixed(key, rec.name);
    append_length_prefixed(key, rec.version ? std::string_view(*rec.version) : std::string_view{});
    append_length_prefixed(key, rec.architecture ? std::string_view(*rec.architecture) : std::string_view{});
    append_length_prefixed(key, rec.install_location ? std::string_view(*rec.install_location) : std::string_view{});
    append_length_prefixed(key, std::to_string(static_cast<int>(rec.format)));
    return key;
}

/// Constructs a strictly collision-free deduplication key for runtime records using length-prefix encoding.
inline std::string make_runtime_dedup_key(const runtime_record& rec) {
    std::string key;
    append_length_prefixed(key, std::to_string(static_cast<int>(rec.kind)));
    append_length_prefixed(key, rec.name);
    append_length_prefixed(key, rec.version);
    append_length_prefixed(key, rec.installation_path);
    append_length_prefixed(key, rec.architecture ? std::string_view(*rec.architecture) : std::string_view{});
    return key;
}

} // namespace software_common
} // namespace detail
} // namespace syscape

#endif
