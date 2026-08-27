#ifndef SYSCAPE_DETAIL_POWER_COMMON_HPP
#define SYSCAPE_DETAIL_POWER_COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_common {

using battery_condition = ::syscape::power::battery_state;
using battery_health = ::syscape::power::battery_health;
using battery_technology = ::syscape::power::battery_technology;
using power_source_type = ::syscape::power::power_source_type;

/// Tri-state outcome of an external-power-source query, shared by the
/// Hosted backends awaiting boundary conversion.
///
/// Platforms record external power as a three-valued fact: connected,
/// disconnected, or no usable evidence. The public boundary turns
/// no_evidence into the portable not_found error instead of fabricating a
/// Boolean answer.
enum class external_presence {
    /// The platform exposes no acceptable evidence either way.
    no_evidence,
    /// The platform reports at least one active external source.
    connected,
    /// The platform reports that the system draws no external power.
    disconnected
};

/// One recorded battery snapshot shared by the Hosted backends awaiting
/// boundary conversion.
struct battery_record {
    /// Verbatim platform label. An empty identifier is valid data that means
    /// the platform records no name for the battery, for example when it
    /// aggregates several physical packs into one logical view.
    std::string identifier;
    /// Whether the platform reports the battery as physically present.
    bool present = true;
    /// Recorded operating condition.
    battery_condition condition = battery_condition::unknown;
    /// Whether the platform exposed a charge estimate in percent.
    bool has_charge_percent = false;
    /// Platform charge estimate from zero through one hundred.
    std::uint32_t charge_percent = 0U;
    /// Whether the platform tracks completed charge cycles.
    bool has_cycle_count = false;
    /// Completed charge and discharge cycles recorded by the platform.
    std::uint64_t cycle_count = 0U;

    /// Recorded health condition.
    battery_health health = battery_health::unknown;
    /// Chemical technology of the battery.
    battery_technology technology = battery_technology::unknown;
    /// Manufacturer or vendor name if reported.
    std::string manufacturer;
    /// Model name or part number if reported.
    std::string model_name;
    /// Serial number if reported.
    std::string serial_number;

    /// Whether health relative to design capacity in percent is available.
    bool has_health_percent = false;
    /// Health percentage (ratio of full charge capacity to design capacity * 100).
    std::uint32_t health_percent = 0U;

    /// Whether design energy capacity in milliwatt-hours is available.
    bool has_energy_design_mwh = false;
    /// Design energy capacity in milliwatt-hours.
    std::uint64_t energy_design_mwh = 0U;

    /// Whether full charge energy capacity in milliwatt-hours is available.
    bool has_energy_full_mwh = false;
    /// Last full charge energy capacity in milliwatt-hours.
    std::uint64_t energy_full_mwh = 0U;

    /// Whether current remaining energy in milliwatt-hours is available.
    bool has_energy_now_mwh = false;
    /// Current remaining energy in milliwatt-hours.
    std::uint64_t energy_now_mwh = 0U;

    /// Whether design charge capacity in milliampere-hours is available.
    bool has_charge_design_mah = false;
    /// Design charge capacity in milliampere-hours.
    std::uint64_t charge_design_mah = 0U;

    /// Whether full charge capacity in milliampere-hours is available.
    bool has_charge_full_mah = false;
    /// Last full charge capacity in milliampere-hours.
    std::uint64_t charge_full_mah = 0U;

    /// Whether current remaining charge in milliampere-hours is available.
    bool has_charge_now_mah = false;
    /// Current remaining charge in milliampere-hours.
    std::uint64_t charge_now_mah = 0U;

    /// Whether battery terminal voltage in millivolts is available.
    bool has_voltage_millivolts = false;
    /// Battery terminal voltage in millivolts.
    std::uint32_t voltage_millivolts = 0U;

    /// Whether minimum design voltage in millivolts is available.
    bool has_voltage_min_design_millivolts = false;
    /// Minimum design voltage in millivolts.
    std::uint32_t voltage_min_design_millivolts = 0U;

    /// Whether power rate in milliwatts is available.
    bool has_power_rate_milliwatts = false;
    /// Power rate in milliwatts (positive = charging, negative = discharging).
    std::int64_t power_rate_milliwatts = 0;

    /// Whether battery temperature in degrees Celsius is available.
    bool has_temperature_celsius = false;
    /// Battery temperature in degrees Celsius.
    double temperature_celsius = 0.0;
};

/// One recorded power source snapshot shared by the Hosted backends.
struct power_source_record {
    std::string identifier;
    power_source_type type = power_source_type::unknown;
    bool has_online = false;
    bool online = false;
    std::string description;

    bool has_max_voltage_millivolts = false;
    std::uint32_t max_voltage_millivolts = 0U;

    bool has_max_power_milliwatts = false;
    std::uint32_t max_power_milliwatts = 0U;
};

/// Validates converted battery entries at the public boundary.
///
/// Identifiers must be well-formed UTF-8, because Hosted Full text is UTF-8
/// by contract. A charge estimate outside the documented zero-to-one-
/// hundred range contradicts every platform's definition of the field and
/// is malformed platform data rather than an unusual reading.
inline result<std::vector<battery_record>> validate_battery_records(
    result<std::vector<battery_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const battery_record& record : *records) {
        if (!is_valid_utf8(record.identifier) ||
            !is_valid_utf8(record.manufacturer) ||
            !is_valid_utf8(record.model_name) ||
            !is_valid_utf8(record.serial_number)) {
            return fail(errc::invalid_encoding);
        }
        if (record.has_charge_percent && record.charge_percent > 100U) {
            return fail(errc::malformed_data);
        }
    }
    return records;
}

/// Validates converted power source entries at the public boundary.
inline result<std::vector<power_source_record>> validate_power_source_records(
    result<std::vector<power_source_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const power_source_record& record : *records) {
        if (!is_valid_utf8(record.identifier) ||
            !is_valid_utf8(record.description)) {
            return fail(errc::invalid_encoding);
        }
    }
    return records;
}

} // namespace power_common
} // namespace detail
} // namespace syscape

#endif
