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

/// Recorded health status of one battery.
enum class battery_health : std::uint8_t {
    /// The platform records no usable health condition.
    unknown,
    /// The platform records the battery health as good or normal.
    good,
    /// The battery is dead or permanently failed.
    dead,
    /// The battery is experiencing an overheat condition.
    overheat,
    /// The battery is experiencing an over-voltage condition.
    over_voltage,
    /// The battery is experiencing an under-voltage condition.
    under_voltage,
    /// The battery has a blown fuse.
    blown_fuse,
    /// The battery cells are imbalanced.
    cell_imbalance,
    /// The battery has suffered an unspecified failure.
    unspecified_failure,
    /// The battery temperature is cold.
    cold,
    /// The battery temperature is warm.
    warm,
    /// The battery temperature is cool.
    cool,
    /// The battery temperature is hot.
    hot
};

/// Chemical technology of one battery.
enum class battery_technology : std::uint8_t {
    /// The platform records no usable technology classification.
    unknown,
    /// Lithium-ion (Li-ion).
    lithium_ion,
    /// Lithium-polymer (Li-poly).
    lithium_polymer,
    /// Nickel-metal hydride (NiMH).
    nickel_metal_hydride,
    /// Nickel-cadmium (NiCd).
    nickel_cadmium,
    /// Lead-acid.
    lead_acid,
    /// Other or proprietary chemistry.
    other
};

/// Classification of a system power supply source.
enum class power_source_type : std::uint8_t {
    /// Unclassified or unknown power supply.
    unknown,
    /// AC / Mains line power adapter.
    mains,
    /// Standard USB power.
    usb,
    /// USB Power Delivery (USB-PD).
    usb_pd,
    /// Wireless / inductive power supply.
    wireless,
    /// Uninterruptible Power Supply (UPS).
    ups,
    /// Other external power source.
    other
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
    std::string identifier{};
    /// Whether the platform reports the battery as physically present.
    bool present = true;
    /// Recorded operating condition at the moment of the query.
    battery_state state = battery_state::unknown;
    /// Whether the platform exposed a charge estimate for this battery.
    bool has_charge_percent = false;
    /// Platform charge estimate from zero through one hundred percent,
    /// meaningful only when has_charge_percent is true.
    std::uint32_t charge_percent = 0U;
    /// Whether the platform tracks completed charge cycles for this battery.
    bool has_cycle_count = false;
    /// Completed charge and discharge cycles recorded by the platform,
    /// meaningful only when has_cycle_count is true.
    std::uint64_t cycle_count = 0U;

    /// Recorded health status at the moment of the query.
    battery_health health = battery_health::unknown;
    /// Chemical technology of the battery.
    battery_technology technology = battery_technology::unknown;
    /// Manufacturer or vendor name if reported.
    std::string manufacturer{};
    /// Model name or part number if reported.
    std::string model_name{};
    /// Serial number if reported.
    std::string serial_number{};

    /// Whether the platform exposed battery health / degradation relative to
    /// design capacity.
    bool has_health_percent = false;
    /// Battery health relative to design capacity in whole percent (ratio of
    /// full charge capacity to design capacity multiplied by 100), meaningful
    /// only when has_health_percent is true.
    std::uint32_t health_percent = 0U;

    /// Whether design energy capacity in milliwatt-hours is available.
    bool has_energy_design_mwh = false;
    /// Design energy capacity in milliwatt-hours (mWh).
    std::uint64_t energy_design_mwh = 0U;

    /// Whether last full charge energy capacity in milliwatt-hours is available.
    bool has_energy_full_mwh = false;
    /// Last full charge energy capacity in milliwatt-hours (mWh).
    std::uint64_t energy_full_mwh = 0U;

    /// Whether current remaining energy in milliwatt-hours is available.
    bool has_energy_now_mwh = false;
    /// Current remaining energy in milliwatt-hours (mWh).
    std::uint64_t energy_now_mwh = 0U;

    /// Whether design charge capacity in milliampere-hours is available.
    bool has_charge_design_mah = false;
    /// Design charge capacity in milliampere-hours (mAh).
    std::uint64_t charge_design_mah = 0U;

    /// Whether last full charge capacity in milliampere-hours is available.
    bool has_charge_full_mah = false;
    /// Last full charge capacity in milliampere-hours (mAh).
    std::uint64_t charge_full_mah = 0U;

    /// Whether current remaining charge in milliampere-hours is available.
    bool has_charge_now_mah = false;
    /// Current remaining charge in milliampere-hours (mAh).
    std::uint64_t charge_now_mah = 0U;

    /// Whether instantaneous battery terminal voltage in millivolts is available.
    bool has_voltage_millivolts = false;
    /// Instantaneous battery terminal voltage in millivolts (mV).
    std::uint32_t voltage_millivolts = 0U;

    /// Whether minimum design voltage in millivolts is available.
    bool has_voltage_min_design_millivolts = false;
    /// Minimum design voltage in millivolts (mV).
    std::uint32_t voltage_min_design_millivolts = 0U;

    /// Whether instantaneous power rate in milliwatts is available.
    bool has_power_rate_milliwatts = false;
    /// Instantaneous power rate in milliwatts (mW). Positive when receiving
    /// charge, negative when discharging, and zero when resting.
    std::int64_t power_rate_milliwatts = 0;

    /// Whether battery temperature in degrees Celsius is available.
    bool has_temperature_celsius = false;
    /// Battery temperature in degrees Celsius (°C).
    double temperature_celsius = 0.0;
};

/// One power supply source snapshot reported by the platform.
struct power_source_entry {
    /// Verbatim platform label rendered as UTF-8.
    std::string identifier{};
    /// Classification of the power source.
    power_source_type type = power_source_type::unknown;
    /// Whether the online state was exposed by the platform.
    bool has_online = false;
    /// Whether the power source is actively online and providing power,
    /// meaningful only when has_online is true.
    bool online = false;
    /// Model name or description of the supply if available.
    std::string description{};

    /// Whether maximum supply voltage in millivolts is available.
    bool has_max_voltage_millivolts = false;
    /// Maximum supply voltage in millivolts (mV).
    std::uint32_t max_voltage_millivolts = 0U;

    /// Whether maximum supply power in milliwatts is available.
    bool has_max_power_milliwatts = false;
    /// Maximum supply power in milliwatts (mW).
    std::uint32_t max_power_milliwatts = 0U;
};

/// Returns one entry per battery recorded by the platform.
///
/// Only batteries satisfy this contract: external adapters and uninterruptible
/// power supplies participate in power_sources() and external_power_online()
/// instead. Entries are ordered by ascending identifier so an unchanged
/// population always enumerates identically. An empty vector is valid data
/// that means the platform records no battery, which is ordinary on desktop
/// and server hardware. Every field can change between calls as batteries are
/// added, removed, or used.
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

        entry.health = record.health;
        entry.technology = record.technology;
        entry.manufacturer = record.manufacturer;
        entry.model_name = record.model_name;
        entry.serial_number = record.serial_number;

        entry.has_health_percent = record.has_health_percent;
        entry.health_percent = record.health_percent;

        entry.has_energy_design_mwh = record.has_energy_design_mwh;
        entry.energy_design_mwh = record.energy_design_mwh;

        entry.has_energy_full_mwh = record.has_energy_full_mwh;
        entry.energy_full_mwh = record.energy_full_mwh;

        entry.has_energy_now_mwh = record.has_energy_now_mwh;
        entry.energy_now_mwh = record.energy_now_mwh;

        entry.has_charge_design_mah = record.has_charge_design_mah;
        entry.charge_design_mah = record.charge_design_mah;

        entry.has_charge_full_mah = record.has_charge_full_mah;
        entry.charge_full_mah = record.charge_full_mah;

        entry.has_charge_now_mah = record.has_charge_now_mah;
        entry.charge_now_mah = record.charge_now_mah;

        entry.has_voltage_millivolts = record.has_voltage_millivolts;
        entry.voltage_millivolts = record.voltage_millivolts;

        entry.has_voltage_min_design_millivolts =
            record.has_voltage_min_design_millivolts;
        entry.voltage_min_design_millivolts =
            record.voltage_min_design_millivolts;

        entry.has_power_rate_milliwatts = record.has_power_rate_milliwatts;
        entry.power_rate_milliwatts = record.power_rate_milliwatts;

        entry.has_temperature_celsius = record.has_temperature_celsius;
        entry.temperature_celsius = record.temperature_celsius;

        output.push_back(std::move(entry));
    }
    return output;
}

/// Returns one entry per external power supply recorded by the platform.
///
/// Power sources include mains / AC adapters, USB-PD supplies, wireless
/// chargers, and UPS units. Entries are ordered by ascending identifier.
/// An empty vector is valid data indicating no external supplies were
/// discovered.
/// @return Zero or more entries, not_supported if the platform provides no
/// power supply query, or a native error.
inline result<std::vector<power_source_entry>> power_sources() {
    const result<std::vector<detail::power_common::power_source_record>> records =
        detail::power_common::validate_power_source_records(
            detail::power_backend::power_sources());
    if (!records) { return fail(records.error()); }
    std::vector<power_source_entry> output;
    output.reserve(records->size());
    for (const detail::power_common::power_source_record& record : *records) {
        power_source_entry entry;
        entry.identifier = record.identifier;
        entry.type = record.type;
        entry.has_online = record.has_online;
        entry.online = record.online;
        entry.description = record.description;
        entry.has_max_voltage_millivolts = record.has_max_voltage_millivolts;
        entry.max_voltage_millivolts = record.max_voltage_millivolts;
        entry.has_max_power_milliwatts = record.has_max_power_milliwatts;
        entry.max_power_milliwatts = record.max_power_milliwatts;
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
