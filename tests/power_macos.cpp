#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/power.hpp>
#include <syscape/detail/power/macos.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

syscape::detail::power_backend::power_source_facts make_battery(
    const std::string& name) {
    syscape::detail::power_backend::power_source_facts facts;
    facts.type = syscape::detail::power_backend::internal_battery_type;
    facts.has_state = true;
    facts.state = syscape::detail::power_backend::ac_power_state;
    facts.has_name = true;
    facts.name = name;
    facts.is_present = true;
    facts.is_charging = false;
    return facts;
}

void test_battery_interpretation() {
    namespace backend = syscape::detail::power_backend;
    using syscape::detail::power_common::battery_condition;

    std::vector<backend::power_source_facts> sources;
    backend::power_source_facts ups;
    ups.type = backend::ups_type;
    ups.has_state = true;
    ups.state = backend::ac_power_state;
    sources.push_back(std::move(ups));

    backend::power_source_facts battery = make_battery("InternalBattery-0");
    battery.is_charging = true;
    battery.has_charge_percent = true;
    battery.charge_percent = 42.5;
    sources.push_back(std::move(battery));

    const auto records = backend::interpret_batteries(sources);
    expect(records && records->size() == 1U,
           "Only internal batteries satisfy the portable battery "
           "contract");
    if (!records || records->empty()) { return; }

    expect(records->front().identifier == "InternalBattery-0" &&
               records->front().condition == battery_condition::charging &&
               records->front().has_charge_percent &&
               records->front().charge_percent == 43U &&
               !records->front().has_cycle_count,
           "The recorded charging flag and rounded estimate must surface "
           "without inventing unavailable cycle accounting");

    std::vector<backend::power_source_facts> unordered;
    unordered.push_back(make_battery("InternalBattery-2"));
    unordered.push_back(make_battery("InternalBattery-0"));
    unordered.push_back(make_battery("InternalBattery-1"));
    const auto ordered = backend::interpret_batteries(unordered);
    expect(ordered && ordered->size() == 3U &&
               (*ordered)[0].identifier == "InternalBattery-0" &&
               (*ordered)[1].identifier == "InternalBattery-1" &&
               (*ordered)[2].identifier == "InternalBattery-2",
           "Battery records must be ordered by ascending identifier");

    std::vector<backend::power_source_facts> discharging;
    backend::power_source_facts off_power =
        make_battery("InternalBattery-0");
    off_power.state = backend::battery_power_state;
    off_power.has_charge_percent = true;
    off_power.charge_percent = 12.0;
    discharging.push_back(std::move(off_power));

    const auto draining = backend::interpret_batteries(discharging);
    expect(draining && draining->front().condition ==
                           battery_condition::discharging,
           "A source drawing battery power must mean discharging");

    std::vector<backend::power_source_facts> resting;
    backend::power_source_facts on_power = make_battery("InternalBattery-0");
    resting.push_back(std::move(on_power));

    const auto plugged = backend::interpret_batteries(resting);
    expect(plugged && plugged->front().condition ==
                          battery_condition::not_charging,
           "AC power without the charging flag must mean resting");

    std::vector<backend::power_source_facts> full;
    backend::power_source_facts charged = make_battery("InternalBattery-0");
    charged.is_charged = true;
    full.push_back(std::move(charged));

    const auto completed = backend::interpret_batteries(full);
    expect(completed && completed->front().condition ==
                            battery_condition::full,
           "The documented charged flag must mean fully charged");

    std::vector<backend::power_source_facts> unnamed;
    backend::power_source_facts anonymous = make_battery("");
    anonymous.has_name = false;
    anonymous.is_present = false;
    unnamed.push_back(std::move(anonymous));

    const auto unlabeled = backend::interpret_batteries(unnamed);
    expect(unlabeled && unlabeled->front().identifier.empty() &&
               !unlabeled->front().present,
           "An absent name records an empty identifier, which is valid "
           "data");

    std::vector<backend::power_source_facts> impossible;
    backend::power_source_facts overfull = make_battery("B");
    overfull.has_charge_percent = true;
    overfull.charge_percent = 120.0;
    impossible.push_back(std::move(overfull));

    const auto rejected = backend::interpret_batteries(impossible);
    expect(!rejected && rejected.error() == syscape::errc::malformed_data,
           "An estimate beyond the documented scale must be malformed "
           "platform data");

}

void test_presence_and_runtime_interpretation() {
    namespace backend = syscape::detail::power_backend;
    using presence = syscape::detail::power_common::external_presence;

    std::vector<backend::power_source_facts> ac_only;
    backend::power_source_facts adapter;
    adapter.type = backend::internal_battery_type;
    adapter.has_state = true;
    adapter.state = backend::ac_power_state;
    ac_only.push_back(std::move(adapter));
    const auto connected =
        backend::interpret_external_presence(ac_only);
    expect(connected == presence::connected,
           "A source drawing AC power must mean connected");

    std::vector<backend::power_source_facts> battery_only;
    backend::power_source_facts draining = make_battery("B");
    draining.state = backend::battery_power_state;
    battery_only.push_back(std::move(draining));
    const auto disconnected =
        backend::interpret_external_presence(battery_only);
    expect(disconnected == presence::disconnected,
           "A discharging battery must mean no external power");

    std::vector<backend::power_source_facts> silent;
    backend::power_source_facts opaque;
    opaque.type = backend::ups_type;
    silent.push_back(std::move(opaque));
    const auto unevidenced =
        backend::interpret_external_presence(silent);
    expect(unevidenced == presence::no_evidence,
           "Sources without usable states must contribute no evidence");

    const auto malformed_duration =
        backend::interpret_minutes_as_seconds(
            std::numeric_limits<double>::quiet_NaN());
    expect(!malformed_duration &&
               malformed_duration.error() == syscape::errc::malformed_data,
           "A nonnumeric duration rendering must be malformed");

    const auto negative_duration =
        backend::interpret_minutes_as_seconds(-30.0);
    expect(!negative_duration &&
               negative_duration.error() == syscape::errc::malformed_data,
           "A negative duration rendering must be malformed");

    const auto oversized_duration =
        backend::interpret_minutes_as_seconds(1.0e300);
    expect(!oversized_duration &&
               oversized_duration.error() == syscape::errc::value_too_large,
           "Unrepresentable durations must report value_too_large");

    const auto exact = backend::interpret_minutes_as_seconds(42.5);
    expect(exact && *exact == 2550U,
           "Minute estimates must convert to seconds exactly once");
}

void test_runtime_estimates() {
    namespace backend = syscape::detail::power_backend;

    std::vector<backend::power_source_facts> recalculating;
    backend::power_source_facts estimating = make_battery("B");
    estimating.has_time_remaining_minutes = true;
    estimating.time_remaining_minutes =
        backend::time_remaining_calculating;
    recalculating.push_back(std::move(estimating));

    const auto unavailable =
        backend::interpret_seconds_remaining(recalculating);
    expect(!unavailable &&
               unavailable.error() ==
                   syscape::errc::temporarily_unavailable,
           "A calculating platform estimate must be temporarily "
           "unavailable");

    std::vector<backend::power_source_facts> bounded;
    backend::power_source_facts primary = make_battery("B1");
    bounded.push_back(std::move(primary));
    const auto unestimated =
        backend::interpret_seconds_remaining(bounded);
    expect(!unestimated && unestimated.error() == syscape::errc::not_found,
           "A population without estimates must report not_found rather "
           "than an invented duration");

    backend::power_source_facts secondary = make_battery("B2");
    secondary.has_time_remaining_minutes = true;
    secondary.time_remaining_minutes = 90.25;
    bounded.push_back(std::move(secondary));
    const auto minimum = backend::interpret_seconds_remaining(bounded);
    expect(minimum && *minimum == 5415U,
           "The smallest recorded estimate must bound the system runtime");

    std::vector<backend::power_source_facts> malformed;
    backend::power_source_facts invalid = make_battery("B3");
    invalid.has_time_remaining_minutes = true;
    invalid.time_remaining_minutes = -2.0;
    malformed.push_back(std::move(invalid));
    const auto rejected = backend::interpret_seconds_remaining(malformed);
    expect(!rejected &&
               rejected.error() == syscape::errc::malformed_data,
           "An undocumented negative time-to-empty value must be malformed");

    std::vector<backend::power_source_facts> through_ups;
    backend::power_source_facts supply;
    supply.type = backend::ups_type;
    supply.has_time_remaining_minutes = true;
    supply.time_remaining_minutes = 10.0;
    through_ups.push_back(std::move(supply));
    const auto ups_bound =
        backend::interpret_seconds_remaining(through_ups);
    expect(ups_bound && *ups_bound == 600U,
           "UPS estimates bound the system runtime too");
}

} // namespace

int main() {
    test_battery_interpretation();
    test_presence_and_runtime_interpretation();
    test_runtime_estimates();
    return failures == 0 ? 0 : 1;
}
