#ifndef SYSCAPE_DETAIL_POWER_MACOS_HPP
#define SYSCAPE_DETAIL_POWER_MACOS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <vector>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/ps/IOPSKeys.h>
#include <IOKit/ps/IOPowerSources.h>

#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

/// The documented type rendering of an internal battery.
constexpr const char* internal_battery_type = "InternalBattery";

/// The documented type rendering of an uninterruptible power supply.
constexpr const char* ups_type = "UPS";

/// Documented kIOPSPowerSourceStateKey renderings.
constexpr const char* ac_power_state = "AC Power";
constexpr const char* battery_power_state = "Battery Power";

/// Documented kIOPSTimeToEmptyKey calculating rendering.
constexpr double time_remaining_calculating = -1.0;

/// One power-source description reduced to plain documented values.
///
/// The extraction layer fills the fields whose keys the description
/// carries; absent keys leave the corresponding has-flags false so the
/// interpretation layer can distinguish an unrecorded value from a
/// recorded zero.
struct power_source_facts {
    /// kIOPSTypeKey rendering.
    std::string type;
    /// kIOPSPowerSourceStateKey rendering.
    bool has_state = false;
    std::string state;
    /// kIOPSNameKey rendering.
    bool has_name = false;
    std::string name;
    /// kIOPSIsPresentKey value; a description without the key records a
    /// present source because the platform listed it.
    bool is_present = true;
    /// kIOPSIsChargingKey value.
    bool is_charging = false;
    /// kIOPSIsChargedKey value.
    bool is_charged = false;
    /// kIOPSCurrentCapacityKey value.
    bool has_charge_percent = false;
    double charge_percent = 0.0;
    /// kIOPSTimeToEmptyKey value in minutes, including the documented
    /// negative special renderings.
    bool has_time_remaining_minutes = false;
    double time_remaining_minutes = 0.0;
};

/// Reports whether one recorded source describes an internal battery.
inline bool is_internal_battery(const power_source_facts& facts) noexcept {
    return facts.type == internal_battery_type;
}

/// Reports whether one recorded source describes a battery-backed supply
/// whose own runtime estimate bounds the system's time to empty.
inline bool is_time_bounding_source(const power_source_facts& facts) noexcept {
    return is_internal_battery(facts) || facts.type == ups_type;
}

/// Maps the documented charging and state fields onto the shared condition.
///
/// The charging flag wins over the state rendering. A source drawing
/// battery power discharges; a source on AC power without the charging
/// flag rests. The documented charged flag records a completed charge.
inline power_common::battery_condition interpret_condition(
    const power_source_facts& facts) noexcept {
    using battery_condition = power_common::battery_condition;
    if (facts.is_charging) { return battery_condition::charging; }
    if (facts.is_charged) { return battery_condition::full; }
    if (facts.has_state && facts.state == battery_power_state) {
        return battery_condition::discharging;
    }
    if (facts.has_state && facts.state == ac_power_state) {
        return battery_condition::not_charging;
    }
    return battery_condition::unknown;
}

/// Converts one recorded charge estimate into a whole percentage.
///
/// Darwin renders the estimate as a number on the documented zero-to-one-
/// hundred scale. Renderings outside that scale are malformed platform
/// data; fractional values round half up exactly once.
inline result<std::uint32_t> interpret_charge_percent(
    const power_source_facts& facts) {
    const double value = facts.charge_percent;
    if (!(value >= 0.0 && value <= 100.0)) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value + 0.5);
}

/// Converts the recorded power sources into battery records.
///
/// Only internal batteries satisfy the portable battery contract; UPS
/// devices participate in the external-power and time-remaining queries
/// instead. The platform name becomes the identifier, and a description
/// without a name records an empty identifier, which is valid data meaning
/// that the platform exposes no label. Records are ordered by ascending
/// identifier to satisfy the portable enumeration contract.
inline result<std::vector<power_common::battery_record>> interpret_batteries(
    const std::vector<power_source_facts>& sources) {
    std::vector<power_common::battery_record> records;
    for (const power_source_facts& facts : sources) {
        if (!is_internal_battery(facts)) { continue; }
        power_common::battery_record record;
        if (facts.has_name) { record.identifier = facts.name; }
        record.present = facts.is_present;
        record.condition = interpret_condition(facts);
        if (facts.has_charge_percent) {
            const result<std::uint32_t> percent =
                interpret_charge_percent(facts);
            if (!percent) { return fail(percent.error()); }
            record.has_charge_percent = true;
            record.charge_percent = *percent;
        }
        records.push_back(std::move(record));
    }
    std::sort(records.begin(), records.end(),
              [](const power_common::battery_record& left,
                 const power_common::battery_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

/// Derives the external-power tri-state from the recorded sources.
///
/// Any source drawing AC power proves a connected external supply, and an
/// internal battery drawing battery power proves its absence. Sources
/// without a usable state rendering contribute no evidence, so a system
/// whose platform records nothing reports no evidence instead of a
/// fabricated answer.
inline power_common::external_presence interpret_external_presence(
    const std::vector<power_source_facts>& sources) noexcept {
    bool saw_connected = false;
    bool saw_disconnected = false;
    for (const power_source_facts& facts : sources) {
        if (facts.has_state && facts.state == ac_power_state) {
            saw_connected = true;
        }
        if (is_internal_battery(facts) && facts.has_state &&
            facts.state == battery_power_state) {
            saw_disconnected = true;
        }
    }
    if (saw_connected) { return power_common::external_presence::connected; }
    if (saw_disconnected) {
        return power_common::external_presence::disconnected;
    }
    return power_common::external_presence::no_evidence;
}

/// Converts one recorded minute-based estimate into whole seconds.
inline result<std::uint64_t> interpret_minutes_as_seconds(double minutes) {
    if (minutes != minutes) { return fail(errc::malformed_data); }
    if (minutes < 0.0) { return fail(errc::malformed_data); }
    constexpr double maximum_minutes =
        static_cast<double>((std::numeric_limits<std::uint64_t>::max)() /
                            60U);
    if (minutes > maximum_minutes) { return fail(errc::value_too_large); }
    return static_cast<std::uint64_t>(minutes * 60.0 + 0.5);
}

/// Converts the recorded time-remaining estimates into one system figure.
///
/// Every battery-backed source bounds the system's runtime, so the
/// smallest finite estimate wins. A source still calculating its estimate
/// makes the whole figure temporarily unavailable, and a population with
/// no estimate at all reports not_found rather than an invented duration.
inline result<std::uint64_t> interpret_seconds_remaining(
    const std::vector<power_source_facts>& sources) {
    bool saw_calculating = false;
    bool saw_estimate = false;
    double smallest_minutes = 0.0;
    for (const power_source_facts& facts : sources) {
        if (!is_time_bounding_source(facts)) { continue; }
        if (!facts.has_time_remaining_minutes) { continue; }
        if (facts.time_remaining_minutes == time_remaining_calculating) {
            saw_calculating = true;
            continue;
        }
        if (facts.time_remaining_minutes < 0.0) {
            return fail(errc::malformed_data);
        }
        if (!saw_estimate || facts.time_remaining_minutes < smallest_minutes) {
            smallest_minutes = facts.time_remaining_minutes;
            saw_estimate = true;
        }
    }
    if (saw_calculating) { return fail(errc::temporarily_unavailable); }
    if (!saw_estimate) { return fail(errc::not_found); }
    return interpret_minutes_as_seconds(smallest_minutes);
}

/// Owns one CoreFoundation object reference for the duration of a query.
class cf_object {
public:
    explicit cf_object(::CFTypeRef value) noexcept : value_(value) {}
    cf_object(const cf_object&) = delete;
    cf_object& operator=(const cf_object&) = delete;
    ~cf_object() {
        if (value_ != nullptr) { ::CFRelease(value_); }
    }

    /// Returns the owned reference.
    ::CFTypeRef get() const noexcept { return value_; }

private:
    ::CFTypeRef value_;
};

/// Copies one CoreFoundation string into UTF-8 storage.
inline result<std::string> copy_utf8_string(::CFStringRef value) {
    if (value == nullptr) { return fail(errc::io_error); }
    const ::CFIndex length = ::CFStringGetLength(value);
    const ::CFIndex maximum = ::CFStringGetMaximumSizeForEncoding(
        length, ::kCFStringEncodingUTF8);
    if (maximum < 0) { return fail(errc::io_error); }
    std::string output;
    output.resize(static_cast<std::size_t>(maximum) + 1U);
    if (!::CFStringGetCString(value, &output[0],
                              static_cast<::CFIndex>(output.size()),
                              ::kCFStringEncodingUTF8)) {
        return fail(errc::invalid_encoding);
    }
    output.resize(std::char_traits<char>::length(output.c_str()));
    return output;
}

/// Reads one CoreFoundation number as a double value.
inline result<double> copy_number_double(::CFNumberRef value) {
    double output = 0.0;
    if (value == nullptr ||
        !::CFNumberGetValue(value, ::kCFNumberDoubleType, &output)) {
        return fail(errc::io_error);
    }
    return output;
}

/// Reads one documented Boolean description value.
inline bool read_boolean(const ::CFDictionaryRef description,
                         ::CFStringRef key, bool fallback = false) noexcept {
    const ::CFBooleanRef flag = static_cast<::CFBooleanRef>(
        ::CFDictionaryGetValue(description, key));
    return flag == nullptr ? fallback : ::CFBooleanGetValue(flag);
}

/// Extracts every recorded power-source description into plain values.
///
/// The IOKit power-sources functions report failures only as null
/// references, which carry no standard category, so they map to io_error.
/// Descriptions without a type key cannot be classified and are skipped;
/// every other absent key records an absent field.
inline result<std::vector<power_source_facts>> collect_power_sources() {
    const cf_object blob(::IOPSCopyPowerSourcesInfo());
    if (blob.get() == nullptr) { return fail(errc::io_error); }
    const cf_object list(::IOPSCopyPowerSourcesList(
        static_cast<::CFDictionaryRef>(blob.get())));
    if (list.get() == nullptr) { return fail(errc::io_error); }

    const ::CFArrayRef sources = static_cast<::CFArrayRef>(list.get());
    const ::CFIndex count = ::CFArrayGetCount(sources);
    std::vector<power_source_facts> collected;
    for (::CFIndex index = 0; index < count; ++index) {
        const ::CFTypeRef source =
            ::CFArrayGetValueAtIndex(sources, index);
        if (source == nullptr) { return fail(errc::io_error); }
        const ::CFDictionaryRef description =
            ::IOPSGetPowerSourceDescription(
                static_cast<::CFDictionaryRef>(blob.get()), source);
        if (description == nullptr) { continue; }

        power_source_facts facts;
        const ::CFStringRef type = static_cast<::CFStringRef>(
            ::CFDictionaryGetValue(description, CFSTR(kIOPSTypeKey)));
        if (type == nullptr) { continue; }
        result<std::string> text = copy_utf8_string(type);
        if (!text) { return fail(text.error()); }
        facts.type = std::move(*text);

        const ::CFStringRef state = static_cast<::CFStringRef>(
            ::CFDictionaryGetValue(
                description, CFSTR(kIOPSPowerSourceStateKey)));
        if (state != nullptr) {
            text = copy_utf8_string(state);
            if (!text) { return fail(text.error()); }
            facts.state = std::move(*text);
            facts.has_state = true;
        }

        const ::CFStringRef name = static_cast<::CFStringRef>(
            ::CFDictionaryGetValue(description, CFSTR(kIOPSNameKey)));
        if (name != nullptr) {
            text = copy_utf8_string(name);
            if (!text) { return fail(text.error()); }
            facts.name = std::move(*text);
            facts.has_name = true;
        }

        facts.is_present =
            read_boolean(description, CFSTR(kIOPSIsPresentKey), true);
        facts.is_charging =
            read_boolean(description, CFSTR(kIOPSIsChargingKey));
        facts.is_charged =
            read_boolean(description, CFSTR(kIOPSIsChargedKey));

        const ::CFNumberRef percent = static_cast<::CFNumberRef>(
            ::CFDictionaryGetValue(
                description, CFSTR(kIOPSCurrentCapacityKey)));
        if (percent != nullptr) {
            result<double> value = copy_number_double(percent);
            if (!value) { return fail(value.error()); }
            facts.charge_percent = *value;
            facts.has_charge_percent = true;
        }

        const ::CFNumberRef minutes = static_cast<::CFNumberRef>(
            ::CFDictionaryGetValue(
                description, CFSTR(kIOPSTimeToEmptyKey)));
        if (minutes != nullptr) {
            result<double> value = copy_number_double(minutes);
            if (!value) { return fail(value.error()); }
            facts.time_remaining_minutes = *value;
            facts.has_time_remaining_minutes = true;
        }

        collected.push_back(std::move(facts));
    }
    return collected;
}

/// Reads one CoreFoundation number as a 64-bit integer value.
inline result<std::int64_t> copy_number_int(::CFNumberRef value) {
    std::int64_t output = 0;
    if (value == nullptr ||
        !::CFNumberGetValue(value, ::kCFNumberSInt64Type, &output)) {
        return fail(errc::io_error);
    }
    return output;
}

struct adapter_facts {
    bool has_adapter = false;
    std::string identifier;
    bool has_watts = false;
    std::uint32_t watts = 0U;
};

inline result<adapter_facts> collect_adapter_details() {
    adapter_facts facts;
    const cf_object details(::IOPSCopyExternalPowerAdapterDetails());
    if (details.get() == nullptr) {
        return facts;
    }
    const ::CFDictionaryRef dict =
        static_cast<::CFDictionaryRef>(details.get());
    facts.has_adapter = true;

    const ::CFTypeRef serial =
        ::CFDictionaryGetValue(dict, CFSTR(kIOPSPowerAdapterSerialNumberKey));
    if (serial != nullptr) {
        if (::CFGetTypeID(serial) == ::CFStringGetTypeID()) {
            result<std::string> text =
                copy_utf8_string(static_cast<::CFStringRef>(serial));
            if (!text) { return fail(text.error()); }
            facts.identifier = std::move(*text);
        } else if (::CFGetTypeID(serial) == ::CFNumberGetTypeID()) {
            result<std::int64_t> num =
                copy_number_int(static_cast<::CFNumberRef>(serial));
            if (!num) { return fail(num.error()); }
            facts.identifier = std::to_string(*num);
        }
    } else {
        const ::CFTypeRef adapter_id =
            ::CFDictionaryGetValue(dict, CFSTR(kIOPSPowerAdapterIDKey));
        if (adapter_id != nullptr &&
            ::CFGetTypeID(adapter_id) == ::CFNumberGetTypeID()) {
            result<std::int64_t> num =
                copy_number_int(static_cast<::CFNumberRef>(adapter_id));
            if (!num) { return fail(num.error()); }
            facts.identifier = std::to_string(*num);
        }
    }

    const ::CFNumberRef watts = static_cast<::CFNumberRef>(
        ::CFDictionaryGetValue(dict, CFSTR(kIOPSPowerAdapterWattsKey)));
    if (watts != nullptr) {
        result<double> val = copy_number_double(watts);
        if (!val) { return fail(val.error()); }
        if (*val < 0.0 || *val > static_cast<double>(std::numeric_limits<std::uint32_t>::max() / 1000U)) {
            return fail(errc::malformed_data);
        }
        facts.has_watts = true;
        facts.watts = static_cast<std::uint32_t>(*val);
    }
    return facts;
}

inline result<std::vector<power_common::power_source_record>>
interpret_power_sources(const std::vector<power_source_facts>& sources,
                        const adapter_facts& adapter = adapter_facts{}) {
    std::vector<power_common::power_source_record> records;
    if (adapter.has_adapter) {
        power_common::power_source_record record;
        record.identifier = adapter.identifier;
        record.type = power_common::power_source_type::mains;
        record.has_online = true;
        record.online = true;
        if (adapter.has_watts) {
            record.has_max_power_milliwatts = true;
            record.max_power_milliwatts = adapter.watts * 1000U;
        }
        records.push_back(std::move(record));
    }
    for (const power_source_facts& facts : sources) {
        if (is_internal_battery(facts)) { continue; }
        power_common::power_source_record record;
        if (facts.has_name) { record.identifier = facts.name; }

        if (facts.type == ups_type) {
            record.type = power_common::power_source_type::ups;
        } else {
            record.type = power_common::power_source_type::mains;
        }
        if (facts.has_state) {
            record.has_online = true;
            record.online = (facts.state == ac_power_state);
        }
        records.push_back(std::move(record));
    }
    std::sort(records.begin(), records.end(),
              [](const power_common::power_source_record& left,
                 const power_common::power_source_record& right) {
                  return left.identifier < right.identifier;
              });
    return records;
}

inline result<std::vector<power_common::battery_record>> batteries() {
    const result<std::vector<power_source_facts>> sources =
        collect_power_sources();
    if (!sources) { return fail(sources.error()); }
    return interpret_batteries(*sources);
}

inline result<std::vector<power_common::power_source_record>> power_sources() {
    const result<std::vector<power_source_facts>> sources =
        collect_power_sources();
    if (!sources) { return fail(sources.error()); }
    const result<adapter_facts> adapter = collect_adapter_details();
    if (!adapter) { return fail(adapter.error()); }
    return interpret_power_sources(*sources, *adapter);
}

inline result<power_common::external_presence> external_power_online() {
    const result<adapter_facts> adapter = collect_adapter_details();
    if (adapter && adapter->has_adapter) {
        return power_common::external_presence::connected;
    }
    const result<std::vector<power_source_facts>> sources =
        collect_power_sources();
    if (!sources) { return fail(sources.error()); }
    return interpret_external_presence(*sources);
}

inline result<std::uint64_t> seconds_until_empty() {
    const result<std::vector<power_source_facts>> sources =
        collect_power_sources();
    if (!sources) { return fail(sources.error()); }
    return interpret_seconds_remaining(*sources);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
