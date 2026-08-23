#ifndef SYSCAPE_DETAIL_POWER_WINDOWS_HPP
#define SYSCAPE_DETAIL_POWER_WINDOWS_HPP

#include <cstdint>
#include <system_error>
#include <vector>

#include <windows.h>

#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

/// Documented ACLineStatus renderings of GetSystemPowerStatus.
constexpr std::uint8_t ac_line_offline = 0U;
constexpr std::uint8_t ac_line_online = 1U;
constexpr std::uint8_t ac_line_unknown = 255U;

/// Documented BatteryFlag bit and marker values of GetSystemPowerStatus.
constexpr std::uint8_t battery_flag_charging = 8U;
constexpr std::uint8_t battery_flag_no_system_battery = 128U;
constexpr std::uint8_t battery_flag_unknown = 255U;

/// Documented BatteryLifePercent marker for an unavailable estimate.
constexpr std::uint8_t battery_percent_unknown = 255U;

/// Documented BatteryLifeTime marker for an unavailable estimate.
///
/// The field is a 32-bit value whose unknown rendering is minus one.
constexpr std::uint32_t battery_seconds_unknown = 0xFFFFFFFFU;

/// One SYSTEM_POWER_STATUS snapshot reduced to plain documented fields.
///
/// GetSystemPowerStatus summarizes every physical battery into one logical
/// system battery; the structure therefore describes the platform's
/// aggregate view rather than any individual pack.
struct power_status_snapshot {
    std::uint8_t ac_line_status = ac_line_unknown;
    std::uint8_t battery_flag = battery_flag_unknown;
    std::uint8_t battery_percent = battery_percent_unknown;
    std::uint32_t battery_seconds_remaining = battery_seconds_unknown;
};

/// Converts the documented aggregate battery view into one record.
///
/// Windows records no label for its aggregated battery, so the identifier
/// is empty, which is valid data meaning that the platform exposes no name.
/// The interface exposes no cycle accounting, so the cycle fields remain
/// absent instead of reporting an invented zero. The condition mapping
/// follows the documented fields: the charging bit wins, an offline AC line
/// means the battery discharges, and an online AC line without the charging
/// bit means the battery rests on external power; the platform exposes no
/// distinct full indicator, so full is never claimed.
inline result<std::vector<power_common::battery_record>> interpret_batteries(
    const power_status_snapshot& snapshot) {
    if ((snapshot.battery_flag & battery_flag_no_system_battery) != 0U) {
        return std::vector<power_common::battery_record>{};
    }
    power_common::battery_record record;
    using battery_condition = power_common::battery_condition;
    if ((snapshot.battery_flag & battery_flag_charging) != 0U) {
        record.condition = battery_condition::charging;
    } else if (snapshot.ac_line_status == ac_line_offline) {
        record.condition = battery_condition::discharging;
    } else if (snapshot.ac_line_status == ac_line_online) {
        record.condition = battery_condition::not_charging;
    } else {
        record.condition = battery_condition::unknown;
    }
    if (snapshot.battery_percent != battery_percent_unknown) {
        record.has_charge_percent = true;
        record.charge_percent = snapshot.battery_percent;
    }
    std::vector<power_common::battery_record> records;
    records.push_back(std::move(record));
    return records;
}

/// Converts the documented AC-line field into the shared tri-state.
inline result<power_common::external_presence> interpret_external_presence(
    const power_status_snapshot& snapshot) {
    if (snapshot.ac_line_status == ac_line_online) {
        return power_common::external_presence::connected;
    }
    if (snapshot.ac_line_status == ac_line_offline) {
        return power_common::external_presence::disconnected;
    }
    return power_common::external_presence::no_evidence;
}

/// Converts the documented remaining-life field into seconds.
///
/// The unknown marker means the platform supplies no estimate, which the
/// portable boundary reports as not_found instead of a fabricated duration.
/// Zero is valid data that describes an exhausted battery.
inline result<std::uint64_t> interpret_seconds_remaining(
    std::uint32_t battery_seconds_remaining) {
    if (battery_seconds_remaining == battery_seconds_unknown) {
        return fail(errc::not_found);
    }
    return static_cast<std::uint64_t>(battery_seconds_remaining);
}

/// Reads one documented GetSystemPowerStatus snapshot.
///
/// The call lives in Kernel32 and needs no additional import library.
inline result<power_status_snapshot> query_power_status() {
    ::SYSTEM_POWER_STATUS status {};
    if (::GetSystemPowerStatus(&status) == FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    power_status_snapshot snapshot;
    snapshot.ac_line_status = status.ACLineStatus;
    snapshot.battery_flag = status.BatteryFlag;
    snapshot.battery_percent = status.BatteryLifePercent;
    snapshot.battery_seconds_remaining = status.BatteryLifeTime;
    return snapshot;
}

inline result<std::vector<power_common::battery_record>> batteries() {
    const result<power_status_snapshot> snapshot = query_power_status();
    if (!snapshot) { return fail(snapshot.error()); }
    // The unknown flag rendering means Windows could not read the battery
    // information at all, so the battery population cannot be established
    // honestly and the query reports not_found instead of guessing.
    if (snapshot->battery_flag == battery_flag_unknown) {
        return fail(errc::not_found);
    }
    return interpret_batteries(*snapshot);
}

inline result<power_common::external_presence> external_power_online() {
    const result<power_status_snapshot> snapshot = query_power_status();
    if (!snapshot) { return fail(snapshot.error()); }
    return interpret_external_presence(*snapshot);
}

inline result<std::uint64_t> seconds_until_empty() {
    const result<power_status_snapshot> snapshot = query_power_status();
    if (!snapshot) { return fail(snapshot.error()); }
    return interpret_seconds_remaining(snapshot->battery_seconds_remaining);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
