#ifndef SYSCAPE_DETAIL_POWER_LINUX_HPP
#define SYSCAPE_DETAIL_POWER_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <charconv>
#include <dirent.h>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

/// Root of the kernel's documented power-supply class interface.
///
/// The attributes read here follow Documentation/ABI/testing/sysfs-class-power,
/// which the kernel documents but has not promoted to its stable ABI
/// classification, so future kernels may evolve the rendered values. Parsing
/// therefore stays strict: recognized attributes with undocumented renderings
/// fail honestly instead of being guessed.
constexpr const char* power_supply_root = "/sys/class/power_supply/";

/// Classification of one power supply against this module's contract.
enum class supply_class {
    /// A rechargeable energy store reported through battery attributes.
    battery,
    /// An external source such as a mains adapter, UPS, dock, or wireless
    /// charger whose online attribute reports whether it powers the system.
    external_source,
};

/// Trims the whitespace that a single sysfs attribute read carries around
/// its value.
inline std::string_view trim_attribute(std::string_view value) noexcept {
    const auto blank = [](char letter) noexcept {
        return letter == ' ' || letter == '\t' || letter == '\r' ||
               letter == '\n';
    };
    while (!value.empty() && blank(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && blank(value.back())) { value.remove_suffix(1U); }
    return value;
}

/// Parses the documented battery status renderings.
inline result<power_common::battery_condition> parse_status(
    std::string_view input) {
    const std::string_view value = trim_attribute(input);
    using battery_condition = power_common::battery_condition;
    if (value == "Charging") { return battery_condition::charging; }
    if (value == "Discharging") { return battery_condition::discharging; }
    if (value == "Not charging") { return battery_condition::not_charging; }
    if (value == "Full") { return battery_condition::full; }
    if (value == "Unknown") { return battery_condition::unknown; }
    return fail(errc::malformed_data);
}

/// Parses the documented battery health status renderings.
inline result<power_common::battery_health> parse_health(
    std::string_view input) {
    const std::string_view value = trim_attribute(input);
    using battery_health = power_common::battery_health;
    if (value == "Good") { return battery_health::good; }
    if (value == "Overheat" || value == "Over heat") {
        return battery_health::overheat;
    }
    if (value == "Dead") { return battery_health::dead; }
    if (value == "Over voltage" || value == "Overvoltage") {
        return battery_health::over_voltage;
    }
    if (value == "Under voltage" || value == "Undervoltage") {
        return battery_health::under_voltage;
    }
    if (value == "Blown fuse") {
        return battery_health::blown_fuse;
    }
    if (value == "Cell imbalance") {
        return battery_health::cell_imbalance;
    }
    if (value == "Unspecified failure" ||
        value == "Safety timer expire" ||
        value == "Watchdog timer expire" ||
        value == "Calibration required" ||
        value == "Over current") {
        return battery_health::unspecified_failure;
    }
    if (value == "Cold") { return battery_health::cold; }
    if (value == "Warm") { return battery_health::warm; }
    if (value == "Cool") { return battery_health::cool; }
    if (value == "Hot") { return battery_health::hot; }
    if (value == "Unknown" || value == "No battery") {
        return battery_health::unknown;
    }
    return fail(errc::malformed_data);
}

/// Parses the documented battery technology renderings.
inline result<power_common::battery_technology> parse_technology(
    std::string_view input) {
    const std::string_view value = trim_attribute(input);
    using battery_technology = power_common::battery_technology;
    if (value == "Li-ion" || value == "Li-Ion" || value == "Lithium-ion") {
        return battery_technology::lithium_ion;
    }
    if (value == "Li-poly" || value == "Li-Poly" || value == "Li-Po" ||
        value == "Lithium-polymer") {
        return battery_technology::lithium_polymer;
    }
    if (value == "NiMH" || value == "Ni-MH") {
        return battery_technology::nickel_metal_hydride;
    }
    if (value == "NiCd" || value == "Ni-Cd") {
        return battery_technology::nickel_cadmium;
    }
    if (value == "Lead-acid" || value == "Pb-acid" || value == "PbAc") {
        return battery_technology::lead_acid;
    }
    if (value == "Unknown") {
        return battery_technology::unknown;
    }
    if (!value.empty()) {
        return battery_technology::other;
    }
    return fail(errc::malformed_data);
}

/// Parses the documented power source type renderings.
inline result<power_common::power_source_type> parse_power_source_type(
    std::string_view input) {
    const std::string_view value = trim_attribute(input);
    using power_source_type = power_common::power_source_type;
    if (value == "Mains") { return power_source_type::mains; }
    if (value == "USB") { return power_source_type::usb; }
    if (value == "Wireless") { return power_source_type::wireless; }
    if (value == "UPS") { return power_source_type::ups; }
    if (value == "Battery") { return fail(errc::malformed_data); }
    if (value == "Unknown") { return power_source_type::unknown; }
    if (!value.empty()) { return power_source_type::other; }
    return fail(errc::malformed_data);
}

/// Parses active USB subtype from sysfs usb_type attribute.
inline power_common::power_source_type parse_usb_power_type(
    std::string_view usb_type_content) {
    const std::string_view trimmed = trim_attribute(usb_type_content);
    // If sysfs outputs bracketed active type, e.g. "[PD]" or "[PD_DRP]" or "[PD_PPS]"
    const auto open_bracket = trimmed.find('[');
    const auto close_bracket = trimmed.find(']', open_bracket);
    if (open_bracket != std::string_view::npos &&
        close_bracket != std::string_view::npos &&
        close_bracket > open_bracket + 1U) {
        const std::string_view active =
            trimmed.substr(open_bracket + 1U, close_bracket - open_bracket - 1U);
        if (active == "PD" || active == "PD_DRP" || active == "PD_PPS" ||
            active == "USB_PD" || active == "USB_PD_DRP") {
            return power_common::power_source_type::usb_pd;
        }
        return power_common::power_source_type::usb;
    }
    // Single string without brackets
    if (trimmed == "PD" || trimmed == "PD_DRP" || trimmed == "PD_PPS" ||
        trimmed == "USB_PD" || trimmed == "USB_PD_DRP" || trimmed == "USB-PD") {
        return power_common::power_source_type::usb_pd;
    }
    return power_common::power_source_type::usb;
}

/// Parses an unsigned 64-bit integer from sysfs text.
inline result<std::uint64_t> parse_u64(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint64_t number = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, number);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return number;
}

/// Parses a signed 64-bit integer from sysfs text.
inline result<std::int64_t> parse_i64(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::int64_t number = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, number);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return number;
}

/// Parses a temperature in degrees Celsius from sysfs tenths of a degree Celsius (0.1 °C).
inline result<double> parse_temperature(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::int64_t raw = 0;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, raw);
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    // Sysfs power_supply temp is documented as tenths of a degree Celsius (0.1 °C).
    return static_cast<double>(raw) / 10.0;
}

/// Parses the documented charge estimate in percent.
inline result<std::uint32_t> parse_capacity_percent(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint32_t percent = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, percent);
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    if (percent > 100U) { return fail(errc::malformed_data); }
    return percent;
}

/// Parses the documented zero-or-one Boolean renderings shared by the
/// present and online attributes.
inline result<bool> parse_flag(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "0") { return false; }
    if (value == "1") { return true; }
    return fail(errc::malformed_data);
}

/// Parses the documented external-supply online state.
inline result<bool> parse_online(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "0") { return false; }
    if (value == "1" || value == "2") { return true; }
    return fail(errc::malformed_data);
}

/// Parses the documented completed-cycle count.
inline result<std::uint64_t> parse_cycle_count(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint64_t count = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, count);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return count;
}

/// Classifies the documented type renderings.
inline result<supply_class> classify_supply_type(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "Battery") { return supply_class::battery; }
    if (value == "Mains" || value == "UPS" || value == "USB" ||
        value == "Wireless" || value == "Unknown") {
        return supply_class::external_source;
    }
    return fail(errc::malformed_data);
}

/// One sysfs attribute read together with its presence.
struct attribute_read {
    /// Whether the attribute file existed during the call.
    bool exists = false;
    /// The raw file content, meaningful only when exists is true.
    std::string content;
};

/// Reads one attribute of one power-supply directory entry.
inline result<attribute_read> read_optional_attribute(
    const std::string& entry, const char* attribute) {
    const std::string path =
        std::string(power_supply_root) + entry + "/" + attribute;
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str());
    if (!content) {
        if (content.error() ==
            std::error_code(ENOENT, std::generic_category())) {
            return attribute_read{};
        }
        return fail(content.error());
    }
    return attribute_read{true, std::move(*content)};
}

/// One assembled battery snapshot together with its presence.
struct battery_snapshot {
    bool recorded = false;
    power_common::battery_record record;
};

/// One assembled power source snapshot together with its presence.
struct power_source_snapshot {
    bool recorded = false;
    power_common::power_source_record record;
};

/// Collects the recorded attributes of one battery directory entry.
inline result<battery_snapshot> collect_battery(const char* name) {
    const std::string entry(name);

    const result<attribute_read> type =
        read_optional_attribute(entry, "type");
    if (!type) { return fail(type.error()); }
    if (!type->exists) { return battery_snapshot{}; }
    const result<supply_class> classified =
        classify_supply_type(type->content);
    if (!classified) { return fail(classified.error()); }
    if (*classified != supply_class::battery) { return battery_snapshot{}; }

    power_common::battery_record record;
    record.identifier = entry;

    const result<attribute_read> present_attribute =
        read_optional_attribute(entry, "present");
    if (!present_attribute) { return fail(present_attribute.error()); }
    if (present_attribute->exists) {
        const result<bool> present = parse_flag(present_attribute->content);
        if (!present) { return fail(present.error()); }
        record.present = *present;
    }

    const result<attribute_read> status_attribute =
        read_optional_attribute(entry, "status");
    if (!status_attribute) { return fail(status_attribute.error()); }
    if (status_attribute->exists) {
        const result<power_common::battery_condition> condition =
            parse_status(status_attribute->content);
        if (!condition) { return fail(condition.error()); }
        record.condition = *condition;
    }

    const result<attribute_read> health_attribute =
        read_optional_attribute(entry, "health");
    if (!health_attribute) { return fail(health_attribute.error()); }
    if (health_attribute->exists) {
        const result<power_common::battery_health> health =
            parse_health(health_attribute->content);
        if (!health) { return fail(health.error()); }
        record.health = *health;
    }

    const result<attribute_read> tech_attribute =
        read_optional_attribute(entry, "technology");
    if (!tech_attribute) { return fail(tech_attribute.error()); }
    if (tech_attribute->exists) {
        const result<power_common::battery_technology> tech =
            parse_technology(tech_attribute->content);
        if (!tech) { return fail(tech.error()); }
        record.technology = *tech;
    }

    const result<attribute_read> mfg_attribute =
        read_optional_attribute(entry, "manufacturer");
    if (!mfg_attribute) { return fail(mfg_attribute.error()); }
    if (mfg_attribute->exists) {
        record.manufacturer = std::string(trim_attribute(mfg_attribute->content));
    }

    const result<attribute_read> model_attribute =
        read_optional_attribute(entry, "model_name");
    if (!model_attribute) { return fail(model_attribute.error()); }
    if (model_attribute->exists) {
        record.model_name = std::string(trim_attribute(model_attribute->content));
    }

    const result<attribute_read> serial_attribute =
        read_optional_attribute(entry, "serial_number");
    if (!serial_attribute) { return fail(serial_attribute.error()); }
    if (serial_attribute->exists) {
        record.serial_number =
            std::string(trim_attribute(serial_attribute->content));
    }

    const result<attribute_read> capacity_attribute =
        read_optional_attribute(entry, "capacity");
    if (!capacity_attribute) { return fail(capacity_attribute.error()); }
    if (capacity_attribute->exists) {
        const result<std::uint32_t> percent =
            parse_capacity_percent(capacity_attribute->content);
        if (!percent) { return fail(percent.error()); }
        record.has_charge_percent = true;
        record.charge_percent = *percent;
    }

    const result<attribute_read> cycle_attribute =
        read_optional_attribute(entry, "cycle_count");
    if (!cycle_attribute) { return fail(cycle_attribute.error()); }
    if (cycle_attribute->exists) {
        const result<std::uint64_t> count =
            parse_cycle_count(cycle_attribute->content);
        if (!count) { return fail(count.error()); }
        if (*count != 0U) {
            record.has_cycle_count = true;
            record.cycle_count = *count;
        }
    }

    // Energy metrics (µWh -> mWh)
    const result<attribute_read> energy_design_attr =
        read_optional_attribute(entry, "energy_full_design");
    if (!energy_design_attr) { return fail(energy_design_attr.error()); }
    if (energy_design_attr->exists) {
        const result<std::uint64_t> uwh = parse_u64(energy_design_attr->content);
        if (!uwh) { return fail(uwh.error()); }
        record.has_energy_design_mwh = true;
        record.energy_design_mwh = *uwh / 1000U;
    }

    const result<attribute_read> energy_full_attr =
        read_optional_attribute(entry, "energy_full");
    if (!energy_full_attr) { return fail(energy_full_attr.error()); }
    if (energy_full_attr->exists) {
        const result<std::uint64_t> uwh = parse_u64(energy_full_attr->content);
        if (!uwh) { return fail(uwh.error()); }
        record.has_energy_full_mwh = true;
        record.energy_full_mwh = *uwh / 1000U;
    }

    const result<attribute_read> energy_now_attr =
        read_optional_attribute(entry, "energy_now");
    if (!energy_now_attr) { return fail(energy_now_attr.error()); }
    if (energy_now_attr->exists) {
        const result<std::uint64_t> uwh = parse_u64(energy_now_attr->content);
        if (!uwh) { return fail(uwh.error()); }
        record.has_energy_now_mwh = true;
        record.energy_now_mwh = *uwh / 1000U;
    }

    // Charge metrics (µAh -> mAh)
    const result<attribute_read> charge_design_attr =
        read_optional_attribute(entry, "charge_full_design");
    if (!charge_design_attr) { return fail(charge_design_attr.error()); }
    if (charge_design_attr->exists) {
        const result<std::uint64_t> uah = parse_u64(charge_design_attr->content);
        if (!uah) { return fail(uah.error()); }
        record.has_charge_design_mah = true;
        record.charge_design_mah = *uah / 1000U;
    }

    const result<attribute_read> charge_full_attr =
        read_optional_attribute(entry, "charge_full");
    if (!charge_full_attr) { return fail(charge_full_attr.error()); }
    if (charge_full_attr->exists) {
        const result<std::uint64_t> uah = parse_u64(charge_full_attr->content);
        if (!uah) { return fail(uah.error()); }
        record.has_charge_full_mah = true;
        record.charge_full_mah = *uah / 1000U;
    }

    const result<attribute_read> charge_now_attr =
        read_optional_attribute(entry, "charge_now");
    if (!charge_now_attr) { return fail(charge_now_attr.error()); }
    if (charge_now_attr->exists) {
        const result<std::uint64_t> uah = parse_u64(charge_now_attr->content);
        if (!uah) { return fail(uah.error()); }
        record.has_charge_now_mah = true;
        record.charge_now_mah = *uah / 1000U;
    }

    // Calculate health degradation percentage if design and full capacities exist
    if (record.has_energy_full_mwh && record.has_energy_design_mwh &&
        record.energy_design_mwh > 0U) {
        record.has_health_percent = true;
        record.health_percent = static_cast<std::uint32_t>(
            (record.energy_full_mwh * 100ULL) / record.energy_design_mwh);
    } else if (record.has_charge_full_mah && record.has_charge_design_mah &&
               record.charge_design_mah > 0U) {
        record.has_health_percent = true;
        record.health_percent = static_cast<std::uint32_t>(
            (record.charge_full_mah * 100ULL) / record.charge_design_mah);
    }

    // Voltage metrics (µV -> mV)
    const result<attribute_read> voltage_attr =
        read_optional_attribute(entry, "voltage_now");
    if (!voltage_attr) { return fail(voltage_attr.error()); }
    std::uint64_t uv_now = 0U;
    if (voltage_attr->exists) {
        const result<std::uint64_t> uv = parse_u64(voltage_attr->content);
        if (!uv) { return fail(uv.error()); }
        uv_now = *uv;
        record.has_voltage_millivolts = true;
        record.voltage_millivolts = static_cast<std::uint32_t>(*uv / 1000U);
    }

    const result<attribute_read> voltage_min_attr =
        read_optional_attribute(entry, "voltage_min_design");
    if (!voltage_min_attr) { return fail(voltage_min_attr.error()); }
    if (voltage_min_attr->exists) {
        const result<std::uint64_t> uv = parse_u64(voltage_min_attr->content);
        if (!uv) { return fail(uv.error()); }
        record.has_voltage_min_design_millivolts = true;
        record.voltage_min_design_millivolts =
            static_cast<std::uint32_t>(*uv / 1000U);
    }

    // Power rate (µW -> mW, or derived from current_now µA * voltage_now µV)
    const result<attribute_read> power_attr =
        read_optional_attribute(entry, "power_now");
    if (!power_attr) { return fail(power_attr.error()); }
    if (power_attr->exists) {
        const result<std::int64_t> raw_uw = parse_i64(power_attr->content);
        if (!raw_uw) { return fail(raw_uw.error()); }
        const std::int64_t mw = *raw_uw / 1000;
        record.has_power_rate_milliwatts = true;
        if (record.condition == power_common::battery_condition::discharging) {
            record.power_rate_milliwatts = mw < 0 ? mw : -mw;
        } else {
            record.power_rate_milliwatts = mw < 0 ? -mw : mw;
        }
    } else {
        const result<attribute_read> current_attr =
            read_optional_attribute(entry, "current_now");
        if (!current_attr) { return fail(current_attr.error()); }
        if (current_attr->exists && uv_now > 0U) {
            const result<std::int64_t> raw_ua = parse_i64(current_attr->content);
            if (!raw_ua) { return fail(raw_ua.error()); }
            const std::uint64_t abs_ua = *raw_ua < 0
                ? static_cast<std::uint64_t>(-*raw_ua)
                : static_cast<std::uint64_t>(*raw_ua);
            const std::int64_t mw = static_cast<std::int64_t>((abs_ua * uv_now) / 1000000000ULL);
            record.has_power_rate_milliwatts = true;
            if (record.condition ==
                power_common::battery_condition::discharging) {
                record.power_rate_milliwatts = -mw;
            } else {
                record.power_rate_milliwatts = mw;
            }
        }
    }

    // Temperature (deci-degrees Celsius -> °C)
    const result<attribute_read> temp_attr =
        read_optional_attribute(entry, "temp");
    if (!temp_attr) { return fail(temp_attr.error()); }
    if (temp_attr->exists) {
        const result<double> deg = parse_temperature(temp_attr->content);
        if (!deg) { return fail(deg.error()); }
        record.has_temperature_celsius = true;
        record.temperature_celsius = *deg;
    }

    battery_snapshot snapshot;
    snapshot.recorded = true;
    snapshot.record = std::move(record);
    return snapshot;
}

/// Collects the recorded attributes of one external power supply entry.
inline result<power_source_snapshot> collect_power_source(const char* name) {
    const std::string entry(name);
    const result<attribute_read> type_attr =
        read_optional_attribute(entry, "type");
    if (!type_attr) { return fail(type_attr.error()); }
    if (!type_attr->exists) { return power_source_snapshot{}; }
    const std::string_view type_str = trim_attribute(type_attr->content);
    if (type_str == "Battery") {
        return power_source_snapshot{};
    }

    power_common::power_source_record record;
    record.identifier = entry;
    if (type_str == "Mains") {
        record.type = power_common::power_source_type::mains;
    } else if (type_str == "UPS") {
        record.type = power_common::power_source_type::ups;
    } else if (type_str == "Wireless") {
        record.type = power_common::power_source_type::wireless;
    } else if (type_str == "USB") {
        record.type = power_common::power_source_type::usb;
        const result<attribute_read> usb_type_attr =
            read_optional_attribute(entry, "usb_type");
        if (!usb_type_attr) { return fail(usb_type_attr.error()); }
        if (usb_type_attr->exists) {
            record.type = parse_usb_power_type(usb_type_attr->content);
        }
    } else if (type_str == "Unknown") {
        record.type = power_common::power_source_type::unknown;
    } else if (!type_str.empty()) {
        record.type = power_common::power_source_type::other;
    } else {
        return fail(errc::malformed_data);
    }

    const result<attribute_read> online_attr =
        read_optional_attribute(entry, "online");
    if (!online_attr) { return fail(online_attr.error()); }
    if (online_attr->exists) {
        const result<bool> active = parse_online(online_attr->content);
        if (!active) { return fail(active.error()); }
        record.has_online = true;
        record.online = *active;
    }

    const result<attribute_read> model_attr =
        read_optional_attribute(entry, "model_name");
    if (!model_attr) { return fail(model_attr.error()); }
    if (model_attr->exists) {
        record.description = std::string(trim_attribute(model_attr->content));
    }

    const result<attribute_read> volt_max_attr =
        read_optional_attribute(entry, "voltage_max");
    if (!volt_max_attr) { return fail(volt_max_attr.error()); }
    if (volt_max_attr->exists) {
        const result<std::uint64_t> uv = parse_u64(volt_max_attr->content);
        if (!uv) { return fail(uv.error()); }
        record.has_max_voltage_millivolts = true;
        record.max_voltage_millivolts =
            static_cast<std::uint32_t>(*uv / 1000U);
    }

    power_source_snapshot snapshot;
    snapshot.recorded = true;
    snapshot.record = std::move(record);
    return snapshot;
}

/// Walks every visible power-supply directory entry.
template <typename Visit>
inline result<void> walk_supplies(Visit visit) {
    linux_platform::directory_handle directory(power_supply_root);
    if (!directory.valid()) {
        const std::error_code error(errno, std::generic_category());
        if (error == std::error_code(ENOENT, std::generic_category())) {
            return {};
        }
        return fail(error);
    }
    for (;;) {
        errno = 0;
        const ::dirent* entry = ::readdir(directory.get());
        if (entry == nullptr) { break; }
        if (entry->d_name[0] == '.') { continue; }
        const result<void> outcome = visit(entry->d_name);
        if (!outcome) { return fail(outcome.error()); }
    }
    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return {};
}

/// Enumerates the batteries visible through the power-supply class.
inline result<std::vector<power_common::battery_record>> batteries() {
    std::vector<power_common::battery_record> records;
    const result<void> walked = walk_supplies(
        [&records](const char* name) -> result<void> {
            const result<battery_snapshot> snapshot = collect_battery(name);
            if (!snapshot) { return fail(snapshot.error()); }
            if (snapshot->recorded) { records.push_back(snapshot->record); }
            return {};
        });
    if (!walked) { return fail(walked.error()); }
    std::sort(records.begin(), records.end(),
              [](const power_common::battery_record& left,
                 const power_common::battery_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Enumerates the external power supplies visible through the power-supply class.
inline result<std::vector<power_common::power_source_record>> power_sources() {
    std::vector<power_common::power_source_record> records;
    const result<void> walked = walk_supplies(
        [&records](const char* name) -> result<void> {
            const result<power_source_snapshot> snapshot =
                collect_power_source(name);
            if (!snapshot) { return fail(snapshot.error()); }
            if (snapshot->recorded) { records.push_back(snapshot->record); }
            return {};
        });
    if (!walked) { return fail(walked.error()); }
    std::sort(records.begin(), records.end(),
              [](const power_common::power_source_record& left,
                 const power_common::power_source_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Derives whether an external source powers the system from the
/// power-supply class.
inline result<power_common::external_presence> external_power_online() {
    bool saw_connected = false;
    bool saw_disconnected = false;
    const result<void> walked = walk_supplies(
        [&](const char* name) -> result<void> {
            const std::string entry(name);
            const result<attribute_read> type =
                read_optional_attribute(entry, "type");
            if (!type) { return fail(type.error()); }
            if (!type->exists) { return {}; }
            const result<supply_class> classified =
                classify_supply_type(type->content);
            if (!classified) { return fail(classified.error()); }

            if (*classified == supply_class::external_source) {
                const result<attribute_read> online =
                    read_optional_attribute(entry, "online");
                if (!online) { return fail(online.error()); }
                if (!online->exists) { return {}; }
                const result<bool> active = parse_online(online->content);
                if (!active) { return fail(active.error()); }
                if (*active) { saw_connected = true; }
                else { saw_disconnected = true; }
                return {};
            }
            if (*classified == supply_class::battery) {
                const result<attribute_read> status =
                    read_optional_attribute(entry, "status");
                if (!status) { return fail(status.error()); }
                if (!status->exists) { return {}; }
                const result<power_common::battery_condition> condition =
                    parse_status(status->content);
                if (!condition) { return fail(condition.error()); }
                if (*condition ==
                    power_common::battery_condition::discharging) {
                    saw_disconnected = true;
                }
            }
            return {};
        });
    if (!walked) { return fail(walked.error()); }
    using power_common::external_presence;
    if (saw_connected) { return external_presence::connected; }
    if (saw_disconnected) { return external_presence::disconnected; }
    return external_presence::no_evidence;
}

/// Returns not_supported because Linux documents no runtime time-to-empty
/// estimate through the power-supply class.
inline result<std::uint64_t> seconds_until_empty() {
    return fail(errc::not_supported);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
