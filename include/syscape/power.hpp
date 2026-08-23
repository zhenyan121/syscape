#ifndef SYSCAPE_POWER_HPP
#define SYSCAPE_POWER_HPP

/// @file
/// @brief Hosted battery state, external-power source, and runtime-to-empty
/// estimate queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note Linux implements every query through the kernel-documented sysfs
/// power-supply class interface under /sys/class/power_supply. That
/// interface is documented but has not been promoted to the kernel's stable
/// ABI classification, so future kernels may evolve the rendered values;
/// unrecognized renderings of documented attributes fail honestly rather
/// than being guessed. Windows implements the queries through the
/// documented GetSystemPowerStatus interface, whose fields summarize every
/// physical battery into one logical system battery without a recorded
/// name; the platform exposes no cycle accounting through that interface.
/// macOS implements the queries through the documented IOKit power-sources
/// interfaces (declared in <IOKit/ps/IOPowerSources.h>), which require
/// linking the IOKit and CoreFoundation frameworks on Apple targets. Other
/// targets use the not-supported fallback.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/power.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace syscape {
namespace power {

/// Recorded operating condition of one battery at the moment of a query.
///
/// The values describe the platform's own recorded condition vocabulary and
/// are not derived from charge levels or instantaneous currents.
enum class battery_state : std::uint8_t {
    /// The platform records no usable operating condition.
    unknown,
    /// The platform records the battery as receiving charge.
    charging,
    /// The platform records the battery as powering the system from stored
    /// energy.
    discharging,
    /// The platform records the battery as connected to external power but
    /// neither receiving charge nor powering the system.
    not_charging,
    /// The platform records the battery as fully charged. Platforms whose
    /// public interfaces expose no distinct full indicator never report
    /// this value instead of approximating one from the charge estimate.
    full
};

} // namespace power
} // namespace syscape

#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/power/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/power/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/power/macos.hpp>
#else
#include <syscape/detail/power/generic.hpp>
#endif

namespace syscape {
namespace power {

/// One battery snapshot reported by the platform.
struct battery_entry {
    /// Verbatim platform label rendered as UTF-8. An empty identifier is
    /// valid data that means the platform records no name, for example when
    /// it aggregates several physical packs into one logical battery.
    std::string identifier;
    /// Whether the platform reports the battery as physically present.
    bool present;
    /// Recorded operating condition at the moment of the query.
    battery_state state;
    /// Whether the platform exposed a charge estimate for this battery.
    bool has_charge_percent;
    /// Platform charge estimate from zero through one hundred, meaningful
    /// only when has_charge_percent is true. The value changes with use.
    std::uint32_t charge_percent;
    /// Whether the platform tracks completed charge cycles for this
    /// battery.
    bool has_cycle_count;
    /// Completed charge and discharge cycles recorded by the platform,
    /// meaningful only when has_cycle_count is true. The count grows over
    /// the battery's service life.
    std::uint64_t cycle_count;
};

/// Returns one entry per battery recorded by the platform.
///
/// Only batteries satisfy this contract: external adapters and uninterruptible
/// power supplies participate in external_power_online() instead. Entries
/// are ordered by ascending identifier so an unchanged population always
/// enumerates identically. An empty vector is valid data that means the
/// platform records no battery, which is ordinary on desktop and server
/// hardware. Every field can change between calls as batteries are added,
/// removed, or used.
/// @return Zero or more entries, not_found when the platform cannot
/// establish its battery population, malformed_data for contradictory
/// platform data, invalid_encoding for unconvertible labels, or a native
/// platform error.
inline result<std::vector<battery_entry>> batteries() {
    const result<std::vector<detail::power_common::battery_record>> records =
        detail::power_common::validate_battery_records(
            detail::power_backend::batteries());
    if (!records) { return fail(records.error()); }
    std::vector<battery_entry> output;
    output.reserve(records->size());
    for (const detail::power_common::battery_record& record : *records) {
        battery_entry entry;
        entry.identifier = record.identifier;
        entry.present = record.present;
        entry.state = record.condition;
        entry.has_charge_percent = record.has_charge_percent;
        entry.charge_percent = record.charge_percent;
        entry.has_cycle_count = record.has_cycle_count;
        entry.cycle_count = record.cycle_count;
        output.push_back(std::move(entry));
    }
    return output;
}

/// Returns whether the platform reports an active external power source.
///
/// External sources are mains adapters, docks, wireless chargers, and
/// uninterruptible power supplies actively feeding the system. Linux reads
/// the documented online attribute of each external supply and treats a
/// discharging battery as proof of the absence of external power; systems
/// exposing no usable evidence report not_found instead of a fabricated
/// answer. Windows reports its documented AC-line status. macOS reports the
/// documented power-source states. The answer can change between calls
/// whenever cables, chargers, or power failures change.
/// @return true when at least one external source powers the system, false
/// when the platform establishes that none does, not_found when no
/// acceptable evidence exists, malformed_data for undocumented renderings,
/// or a native platform error.
inline result<bool> external_power_online() {
    const result<detail::power_common::external_presence> presence =
        detail::power_backend::external_power_online();
    if (!presence) { return fail(presence.error()); }
    switch (*presence) {
    case detail::power_common::external_presence::connected:
        return true;
    case detail::power_common::external_presence::disconnected:
        return false;
    case detail::power_common::external_presence::no_evidence:
        break;
    }
    return fail(errc::not_found);
}

/// Returns the operating system's estimate of remaining battery runtime in
/// seconds.
///
/// The estimate is the platform's own projection for the whole system, not
/// a Syscape calculation. It is volatile by definition: governor decisions,
/// load changes, and charger events continuously reshape it. Zero is valid
/// data that describes an exhausted battery. Linux documents no such
/// estimate through its power-supply class, so Linux reports
/// not_supported rather than deriving a figure no platform source provides.
/// @return A nonnegative second count, not_supported when the platform
/// exposes no acceptable source, not_found while the platform supplies no
/// estimate, temporarily_unavailable while the platform recalculates its
/// estimate, value_too_large for unrepresentable durations, or a native
/// platform error.
inline result<std::uint64_t> seconds_until_empty() {
    return detail::power_backend::seconds_until_empty();
}

} // namespace power
} // namespace syscape

#endif
