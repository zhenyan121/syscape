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
///
/// The kernel documents exactly five status values. Any other rendering of
/// the recognized status attribute is malformed platform data.
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

/// Parses the documented charge estimate in percent.
///
/// The capacity attribute renders a plain whole percentage without a unit
/// suffix. Values beyond one hundred contradict every platform's definition
/// of the field and are malformed platform data.
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
///
/// Zero is offline, while both fixed and programmable online states mean
/// that the supply is actively providing external power.
inline result<bool> parse_online(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "0") { return false; }
    if (value == "1" || value == "2") { return true; }
    return fail(errc::malformed_data);
}

/// Parses the documented completed-cycle count.
///
/// The count renders as a plain nonnegative integer without a suffix.
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
///
/// The kernel documents the type attribute as exactly one of a small closed
/// set. Renderings outside that set cannot be classified honestly, so they
/// are malformed platform data rather than silently ignored supplies.
inline result<supply_class> classify_supply_type(std::string_view input) {
    const std::string_view value = trim_attribute(input);
    if (value == "Battery") { return supply_class::battery; }
    if (value == "Mains" || value == "UPS" || value == "USB" ||
        value == "Wireless") {
        return supply_class::external_source;
    }
    return fail(errc::malformed_data);
}

/// One sysfs attribute read together with its presence.
///
/// Batteries legitimately omit individual attributes, so a missing file
/// records an absent field rather than an error. Only the native failures
/// other than ENOENT propagate.
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
    /// Whether the entry described a battery during the call.
    bool recorded = false;
    /// The assembled record, meaningful only when recorded is true.
    power_common::battery_record record;
};

/// Collects the recorded attributes of one power-supply directory entry.
///
/// Entries whose type attribute disappears between listing and reading are
/// expected removal races and produce an unrecorded snapshot; every other
/// failure propagates unchanged so permission and input problems can never
/// masquerade as a complete enumeration.
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
        // The kernel documents a zero count as "this battery does not
        // support counting cycles", so zero records the absence of tracking
        // instead of a completed-cycle total.
        if (*count != 0U) {
            record.has_cycle_count = true;
            record.cycle_count = *count;
        }
    }

    battery_snapshot snapshot;
    snapshot.recorded = true;
    snapshot.record = std::move(record);
    return snapshot;
}

/// Walks every visible power-supply directory entry.
///
/// A system whose kernel lacks the power-supply class exposes no root
/// directory; that configuration enumerates zero supplies instead of
/// failing, because the absence of batteries is ordinary on servers and
/// embedded boards. All other directory failures propagate.
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
///
/// Entries are ordered by ascending identifier so callers observe one
/// deterministic order for an unchanged population. The snapshot reflects
/// the supplies present during the call; hot-plugged batteries and
/// reconfigured devices become visible only to later calls, and every
/// returned value can change between calls as the recorded state changes.
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

/// Derives whether an external source powers the system from the
/// power-supply class.
///
/// Positive evidence is any external supply whose documented online
/// attribute reports an active source; negative evidence is either only
/// inactive external supplies or a battery whose status records that it is
/// discharging, because a discharging battery by definition powers the
/// system from stored energy. Connected evidence dominates: charge-
/// threshold controllers legitimately report a charging-capable adapter and
/// a temporarily discharging battery at once. A system that exposes no
/// usable evidence reports none instead of fabricating a Boolean answer.
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
///
/// The class exposes instantaneous electrical attributes but no operating
/// system estimate of remaining runtime, so deriving a figure here would
/// fabricate data no platform source provides.
inline result<std::uint64_t> seconds_until_empty() {
    return fail(errc::not_supported);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
