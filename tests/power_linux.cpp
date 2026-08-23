#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/power.hpp>
#include <syscape/detail/power/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_status_parser() {
    namespace backend = syscape::detail::power_backend;
    using condition = syscape::detail::power_common::battery_condition;

    const auto charging = backend::parse_status("Charging\n");
    expect(charging && *charging == condition::charging,
           "The documented Charging rendering must map to charging");

    const auto discharging = backend::parse_status("Discharging\n");
    expect(discharging && *discharging == condition::discharging,
           "The documented Discharging rendering must map to discharging");

    const auto not_charging = backend::parse_status("Not charging");
    expect(not_charging && *not_charging == condition::not_charging,
           "The documented Not-charging rendering must be preserved");

    const auto full = backend::parse_status("Full\n");
    expect(full && *full == condition::full,
           "The documented Full rendering must map to full");

    const auto unknown = backend::parse_status("Unknown");
    expect(unknown && *unknown == condition::unknown,
           "The documented Unknown rendering must map to unknown");

    const auto padded = backend::parse_status("  Full \r\n");
    expect(padded && *padded == condition::full,
           "Surrounding attribute whitespace must be tolerated");

    const auto undocumented = backend::parse_status("Sort of charging");
    expect(!undocumented &&
               undocumented.error() == syscape::errc::malformed_data,
           "An undocumented rendering of the status attribute must be "
           "malformed platform data");

    const auto empty = backend::parse_status("   ");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "An empty status attribute must be malformed");
}

void test_capacity_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto typical = backend::parse_capacity_percent("87\n");
    expect(typical && *typical == 87U,
           "A documented capacity rendering must parse as a percentage");

    const auto zero = backend::parse_capacity_percent("0");
    expect(zero && *zero == 0U,
           "Zero percent is valid data for an exhausted battery");

    const auto full = backend::parse_capacity_percent("100");
    expect(full && *full == 100U,
           "One hundred percent is valid data");

    const auto beyond = backend::parse_capacity_percent("101");
    expect(!beyond && beyond.error() == syscape::errc::malformed_data,
           "A percentage beyond one hundred must be malformed platform "
           "data");

    const auto text = backend::parse_capacity_percent("half\n");
    expect(!text && text.error() == syscape::errc::malformed_data,
           "A nonnumeric capacity must be malformed");

    const auto signed_value = backend::parse_capacity_percent("-5");
    expect(!signed_value &&
               signed_value.error() == syscape::errc::malformed_data,
           "A negative capacity must be malformed");

    const auto suffixed = backend::parse_capacity_percent("50 %");
    expect(!suffixed && suffixed.error() == syscape::errc::malformed_data,
           "Trailing text after a percentage must be malformed");
}

void test_flag_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto set = backend::parse_flag("1\n");
    expect(set && *set, "The documented one rendering must mean true");

    const auto clear = backend::parse_flag("0");
    expect(clear && !*clear, "The documented zero rendering must mean false");

    const auto word = backend::parse_flag("true");
    expect(!word && word.error() == syscape::errc::malformed_data,
           "Only the documented digits are accepted flags");

    const auto other_digit = backend::parse_flag("2");
    expect(!other_digit &&
               other_digit.error() == syscape::errc::malformed_data,
           "Any digit other than zero and one must be malformed");
}

void test_online_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto offline = backend::parse_online("0\n");
    expect(offline && !*offline,
           "The documented offline rendering must mean inactive");

    const auto fixed = backend::parse_online("1");
    expect(fixed && *fixed,
           "A fixed online supply must mean active external power");

    const auto programmable = backend::parse_online("2\n");
    expect(programmable && *programmable,
           "A programmable online supply must mean active external power");

    const auto undocumented = backend::parse_online("3");
    expect(!undocumented &&
               undocumented.error() == syscape::errc::malformed_data,
           "An undocumented online rendering must be malformed");
}

void test_cycle_count_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto counted = backend::parse_cycle_count("17\n");
    expect(counted && *counted == 17U,
           "A documented cycle count must parse");

    const auto fresh = backend::parse_cycle_count("0");
    expect(fresh && *fresh == 0U,
           "A zero rendering parses so callers can apply its documented "
           "no-tracking meaning");

    const auto overflow =
        backend::parse_cycle_count("99999999999999999999999");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Cycle counts beyond 64 bits must report value_too_large");

    const auto trailing = backend::parse_cycle_count("17 cycles");
    expect(!trailing && trailing.error() == syscape::errc::malformed_data,
           "Trailing text after a cycle count must be malformed");
}

void test_type_classifier() {
    namespace backend = syscape::detail::power_backend;
    using supply_class = syscape::detail::power_backend::supply_class;

    const auto battery = backend::classify_supply_type("Battery");
    expect(battery && *battery == supply_class::battery,
           "The documented Battery rendering must classify as a battery");

    const auto mains = backend::classify_supply_type("Mains\n");
    expect(mains && *mains == supply_class::external_source,
           "The documented Mains rendering must classify as external");

    const auto ups = backend::classify_supply_type("UPS");
    expect(ups && *ups == supply_class::external_source,
           "UPS supplies participate as external sources only");

    const auto usb = backend::classify_supply_type("USB\n");
    expect(usb && *usb == supply_class::external_source,
           "USB supplies participate as external sources only");

    const auto wireless = backend::classify_supply_type("Wireless");
    expect(wireless && *wireless == supply_class::external_source,
           "Wireless supplies participate as external sources only");

    const auto unrecognized = backend::classify_supply_type("N/A");
    expect(!unrecognized &&
               unrecognized.error() == syscape::errc::malformed_data,
           "An unclassifiable type rendering must be malformed rather than "
           "silently skipped");
}

void test_boundary_validation() {
    namespace common = syscape::detail::power_common;

    syscape::result<std::vector<common::battery_record>> invalid_encoding(
        std::vector<common::battery_record>{});
    invalid_encoding->push_back(common::battery_record{});
    invalid_encoding->back().identifier = "\xff\xfe";

    const auto encoding_checked =
        common::validate_battery_records(std::move(invalid_encoding));
    expect(!encoding_checked &&
               encoding_checked.error() == syscape::errc::invalid_encoding,
           "Battery identifiers must be well-formed UTF-8");

    syscape::result<std::vector<common::battery_record>> impossible(
        std::vector<common::battery_record>{});
    impossible->push_back(common::battery_record{});
    impossible->back().has_charge_percent = true;
    impossible->back().charge_percent = 101U;

    const auto range_checked =
        common::validate_battery_records(std::move(impossible));
    expect(!range_checked &&
               range_checked.error() == syscape::errc::malformed_data,
           "A charge estimate beyond one hundred must be rejected at the "
           "public boundary");
}

/// Reads one sysfs attribute independently of the backend's reader.
bool independent_attribute(const std::string& entry, const char* attribute,
                           std::string& output) {
    const std::string path = "/sys/class/power_supply/" + entry + "/" +
                             attribute;
    std::ifstream stream(path);
    if (!stream.is_open()) { return false; }
    std::getline(stream, output);
    return true;
}

void test_live_queries() {
    using syscape::errc;

    const syscape::result<std::uint64_t> runtime =
        syscape::power::seconds_until_empty();
    expect(!runtime && runtime.error() == errc::not_supported,
           "Linux documents no time-to-empty source, so the query must "
           "report not_supported");

    const syscape::result<std::vector<syscape::power::battery_entry>>
        listed = syscape::power::batteries();
    expect(listed || listed.error() == errc::not_found,
           "Battery enumeration must succeed or report honestly that the "
           "population cannot be established");
    if (!listed) { return; }

    std::string previous_identifier;
    for (const syscape::power::battery_entry& battery : *listed) {
        expect(battery.identifier != "." && battery.identifier != ".." &&
                   battery.identifier.find('/') == std::string::npos,
               "Battery identifiers must be plain directory names");
        expect(previous_identifier <= battery.identifier,
               "Battery entries must be ordered by ascending identifier");
        previous_identifier = battery.identifier;

        expect(battery.charge_percent <= 100U,
               "A reported charge estimate must stay within the documented "
               "percentage range");
        if (battery.has_cycle_count) {
            expect(battery.cycle_count > 0U,
                   "Linux records zero cycles only when tracking is absent, "
                   "which must surface as no count");
        }
        std::string recorded;
        if (independent_attribute(battery.identifier, "capacity",
                                  recorded)) {
            const int expected = std::stoi(recorded);
            expect(battery.has_charge_percent &&
                       static_cast<int>(battery.charge_percent) == expected,
                   "A reported charge estimate must match an independent "
                   "read of the same sysfs attribute");
        } else {
            expect(!battery.has_charge_percent,
                   "A battery without a capacity attribute must record no "
                   "estimate");
        }

        std::string status;
        if (independent_attribute(battery.identifier, "status", status)) {
            expect(status == "Charging" ||
                       status == "Discharging" ||
                       status == "Not charging" ||
                       status == "Full" || status == "Unknown",
                   "Every reported condition must originate from a "
                   "documented status rendering");
        }
    }

    const syscape::result<bool> powered =
        syscape::power::external_power_online();
    expect(powered || powered.error() == errc::not_found,
           "External-power detection must answer or report missing "
           "evidence explicitly");
}

} // namespace

int main() {
    test_status_parser();
    test_capacity_parser();
    test_flag_parser();
    test_online_parser();
    test_cycle_count_parser();
    test_type_classifier();
    test_boundary_validation();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
