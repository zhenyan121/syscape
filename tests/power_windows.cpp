#include <cstdint>
#include <iostream>
#include <system_error>
#include <vector>

#include <syscape/power.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

syscape::detail::power_backend::power_status_snapshot make_snapshot(
    std::uint8_t ac_line_status, std::uint8_t battery_flag,
    std::uint8_t battery_percent,
    std::uint32_t battery_seconds_remaining) {
    syscape::detail::power_backend::power_status_snapshot snapshot;
    snapshot.ac_line_status = ac_line_status;
    snapshot.battery_flag = battery_flag;
    snapshot.battery_percent = battery_percent;
    snapshot.battery_seconds_remaining = battery_seconds_remaining;
    return snapshot;
}

void test_battery_interpretation() {
    namespace backend = syscape::detail::power_backend;
    using syscape::detail::power_common::battery_condition;

    const auto absent = backend::interpret_batteries(make_snapshot(
        backend::ac_line_online,
        backend::battery_flag_no_system_battery, 0U, 0U));
    expect(absent && absent->empty(),
           "A system without any recorded battery must enumerate none");

    const auto charging = backend::interpret_batteries(
        make_snapshot(backend::ac_line_online, 1U | 2U |
                          backend::battery_flag_charging,
                      45U, 7200U));
    expect(charging && charging->size() == 1U &&
               charging->front().condition == battery_condition::charging &&
               charging->front().has_charge_percent &&
               charging->front().charge_percent == 45U,
           "The documented charging bit must win over every other "
           "condition evidence");

    const auto discharging = backend::interpret_batteries(
        make_snapshot(backend::ac_line_offline, 4U, 3U, 300U));
    expect(discharging && discharging->size() == 1U &&
               discharging->front().condition ==
                   battery_condition::discharging,
           "An offline AC line without the charging bit must mean "
           "discharging");

    const auto resting = backend::interpret_batteries(
        make_snapshot(backend::ac_line_online, 1U, 90U,
                      backend::battery_seconds_unknown));
    expect(resting && resting->front().condition ==
                          battery_condition::not_charging,
           "External power without the charging bit must mean resting");

    const auto opaque = backend::interpret_batteries(
        make_snapshot(backend::ac_line_unknown, 1U,
                      backend::battery_percent_unknown,
                      backend::battery_seconds_unknown));
    expect(opaque && opaque->front().condition ==
                         battery_condition::unknown &&
               !opaque->front().has_charge_percent,
           "Unknown renderings must record an unknown condition and no "
           "estimate instead of invented values");

    const auto aggregate = backend::interpret_batteries(make_snapshot(
        backend::ac_line_online, 8U, 50U, 1000U));
    expect(aggregate && aggregate->size() == 1U &&
               aggregate->front().identifier.empty() &&
               !aggregate->front().has_cycle_count &&
               aggregate->front().present,
           "The aggregated system battery records no label and no cycle "
           "accounting");
}

void test_presence_and_runtime_interpretation() {
    namespace backend = syscape::detail::power_backend;
    using presence = syscape::detail::power_common::external_presence;

    const auto connected = backend::interpret_external_presence(
        make_snapshot(backend::ac_line_online, 0U, 0U, 0U));
    expect(connected && *connected == presence::connected,
           "The documented online rendering must mean connected");

    const auto disconnected = backend::interpret_external_presence(
        make_snapshot(backend::ac_line_offline, 0U, 0U, 0U));
    expect(disconnected && *disconnected == presence::disconnected,
           "The documented offline rendering must mean disconnected");

    const auto unevidenced = backend::interpret_external_presence(
        make_snapshot(backend::ac_line_unknown, 0U, 0U, 0U));
    expect(unevidenced && *unevidenced == presence::no_evidence,
           "The documented unknown rendering must mean missing evidence");

    const auto exhausted = backend::interpret_seconds_remaining(0U);
    expect(exhausted && *exhausted == 0U,
           "Zero remaining seconds is valid data for an exhausted "
           "battery");

    const auto hours = backend::interpret_seconds_remaining(7200U);
    expect(hours && *hours == 7200U,
           "A recorded second count must pass through unchanged");

    const auto unknown_runtime =
        backend::interpret_seconds_remaining(
            backend::battery_seconds_unknown);
    expect(!unknown_runtime &&
               unknown_runtime.error() == syscape::errc::not_found,
           "The documented unknown marker must become not_found rather "
           "than a fabricated duration");
}

void test_power_sources_interpretation() {
    namespace backend = syscape::detail::power_backend;

    const auto sources = backend::power_sources();
    expect(!sources && sources.error() == syscape::errc::not_supported,
           "Windows documents no power source enumeration through "
           "GetSystemPowerStatus, so it must return not_supported");
}

void test_live_queries() {
    const auto powered = syscape::power::external_power_online();
    expect(powered.has_value(),
           "Windows must answer its documented AC-line status");

    const auto runtime = syscape::power::seconds_until_empty();
    expect(runtime || runtime.error() == syscape::errc::not_found,
           "Runtime estimates must answer or report their absence");

    const auto sources = syscape::power::power_sources();
    expect(!sources && sources.error() == syscape::errc::not_supported,
           "Power source enumeration must report not_supported on Windows");

    const auto listed = syscape::power::batteries();
    expect(listed || listed.error() == syscape::errc::not_found,
           "Battery enumeration must answer or report that the population "
           "cannot be established");
    if (listed) {
        expect(listed->size() <= 1U,
               "The documented aggregate view describes at most one "
               "logical battery");
        if (!listed->empty()) {
            expect(listed->front().present,
                   "An enumerated logical battery is present by "
                   "definition");
            expect(listed->front().charge_percent <= 100U,
                   "A reported charge estimate must stay within the "
                   "documented range");
        }
    }
}

} // namespace

int main() {
    test_battery_interpretation();
    test_presence_and_runtime_interpretation();
    test_power_sources_interpretation();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
