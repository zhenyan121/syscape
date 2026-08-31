#ifndef SYSCAPE_DETAIL_POWER_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_POWER_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

inline result<std::vector<power_common::battery_record>> batteries() {
    const result<int> units = read_sysctl_int("hw.acpi.battery.units");
    if (!units) {
        if (units.error() == errc::not_found) {
            return std::vector<power_common::battery_record>();
        }
        return fail(units.error());
    }
    if (*units <= 0) {
        return std::vector<power_common::battery_record>();
    }

    const result<int> state_mask = read_sysctl_int("hw.acpi.battery.state");
    if (!state_mask && state_mask.error() != errc::not_found) {
        return fail(state_mask.error());
    }

    const result<int> life_pct = read_sysctl_int("hw.acpi.battery.life");
    if (!life_pct && life_pct.error() != errc::not_found) {
        return fail(life_pct.error());
    }

    // DragonFly BSD hw.acpi.battery sysctls report aggregate status for all
    // connected units. If exactly 1 unit is present, label it BAT0; if multiple
    // units are aggregated, use an empty identifier representing the logical
    // aggregate view.
    power_common::battery_record rec;
    rec.identifier = (*units == 1) ? "BAT0" : "";
    rec.present = true;

    if (state_mask && *state_mask >= 0) {
        if ((*state_mask & 2) != 0) {
            rec.condition = power_common::battery_condition::charging;
        } else if ((*state_mask & 1) != 0) {
            rec.condition = power_common::battery_condition::discharging;
        } else if (life_pct && *life_pct == 100) {
            rec.condition = power_common::battery_condition::full;
        } else {
            rec.condition = power_common::battery_condition::not_charging;
        }
    }

    if (life_pct && *life_pct >= 0 && *life_pct <= 100) {
        rec.has_charge_percent = true;
        rec.charge_percent = static_cast<std::uint32_t>(*life_pct);
    }

    return std::vector<power_common::battery_record> {std::move(rec)};
}

inline result<std::vector<power_common::power_source_record>> power_sources() {
    const result<int> acline = read_sysctl_int("hw.acpi.acline");
    if (!acline) {
        if (acline.error() == errc::not_found) {
            return std::vector<power_common::power_source_record>();
        }
        return fail(acline.error());
    }

    std::vector<power_common::power_source_record> sources;
    power_common::power_source_record rec;
    rec.identifier = "AC";
    rec.type = power_common::power_source_type::mains;
    rec.has_online = true;
    rec.online = (*acline != 0);
    rec.description = "AC Adapter";
    sources.push_back(std::move(rec));
    return sources;
}

inline result<power_common::external_presence> external_power_online() {
    const result<int> acline = read_sysctl_int("hw.acpi.acline");
    if (!acline) {
        if (acline.error() == errc::not_found) {
            return power_common::external_presence::no_evidence;
        }
        return fail(acline.error());
    }
    return *acline != 0 ? power_common::external_presence::connected
                        : power_common::external_presence::disconnected;
}

inline result<std::uint64_t> seconds_until_empty() {
    const result<int> time_mins = read_sysctl_int("hw.acpi.battery.time");
    if (!time_mins) {
        return fail(time_mins.error());
    }
    if (*time_mins < 0) {
        return fail(errc::not_found);
    }
    return static_cast<std::uint64_t>(*time_mins) * 60ULL;
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
