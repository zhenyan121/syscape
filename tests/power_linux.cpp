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

void test_health_parser() {
    namespace backend = syscape::detail::power_backend;
    using health = syscape::detail::power_common::battery_health;

    const auto good = backend::parse_health("Good\n");
    expect(good && *good == health::good, "Good must map to good");

    const auto overheat = backend::parse_health("Overheat");
    expect(overheat && *overheat == health::overheat,
           "Overheat must map to overheat");

    const auto over_heat = backend::parse_health("Over heat\n");
    expect(over_heat && *over_heat == health::overheat,
           "Over heat must map to overheat");

    const auto dead = backend::parse_health("Dead");
    expect(dead && *dead == health::dead, "Dead must map to dead");

    const auto over_voltage = backend::parse_health("Over voltage");
    expect(over_voltage && *over_voltage == health::over_voltage,
           "Over voltage must map to over_voltage");

    const auto under_voltage = backend::parse_health("Under voltage");
    expect(under_voltage && *under_voltage == health::under_voltage,
           "Under voltage must map to under_voltage");

    const auto blown_fuse = backend::parse_health("Blown fuse");
    expect(blown_fuse && *blown_fuse == health::blown_fuse,
           "Blown fuse must map to blown_fuse");

    const auto cell_imbalance = backend::parse_health("Cell imbalance");
    expect(cell_imbalance && *cell_imbalance == health::cell_imbalance,
           "Cell imbalance must map to cell_imbalance");

    const auto unspecified = backend::parse_health("Unspecified failure");
    expect(unspecified && *unspecified == health::unspecified_failure,
           "Unspecified failure must map to unspecified_failure");

    const auto cold = backend::parse_health("Cold");
    expect(cold && *cold == health::cold, "Cold must map to cold");

    const auto warm = backend::parse_health("Warm");
    expect(warm && *warm == health::warm, "Warm must map to warm");

    const auto cool = backend::parse_health("Cool");
    expect(cool && *cool == health::cool, "Cool must map to cool");

    const auto hot = backend::parse_health("Hot");
    expect(hot && *hot == health::hot, "Hot must map to hot");

    const auto unknown = backend::parse_health("Unknown");
    expect(unknown && *unknown == health::unknown, "Unknown must map to unknown");

    const auto watchdog = backend::parse_health("Watchdog timer expire");
    expect(watchdog && *watchdog == health::unspecified_failure,
           "Watchdog timer expire must map to unspecified_failure");

    const auto no_battery = backend::parse_health("No battery");
    expect(no_battery && *no_battery == health::unknown,
           "No battery must map to unknown");

    const auto invalid = backend::parse_health("Strange");
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Unrecognized health rendering must be malformed");
}

void test_technology_parser() {
    namespace backend = syscape::detail::power_backend;
    using tech = syscape::detail::power_common::battery_technology;

    const auto lion = backend::parse_technology("Li-ion\n");
    expect(lion && *lion == tech::lithium_ion, "Li-ion must map to lithium_ion");

    const auto lipo = backend::parse_technology("Li-poly");
    expect(lipo && *lipo == tech::lithium_polymer,
           "Li-poly must map to lithium_polymer");

    const auto nimh = backend::parse_technology("NiMH");
    expect(nimh && *nimh == tech::nickel_metal_hydride,
           "NiMH must map to nickel_metal_hydride");

    const auto nicd = backend::parse_technology("NiCd");
    expect(nicd && *nicd == tech::nickel_cadmium,
           "NiCd must map to nickel_cadmium");

    const auto lead = backend::parse_technology("Lead-acid");
    expect(lead && *lead == tech::lead_acid, "Lead-acid must map to lead_acid");

    const auto unknown = backend::parse_technology("Unknown");
    expect(unknown && *unknown == tech::unknown,
           "Unknown must map to unknown");

    const auto other = backend::parse_technology("Custom-Chem");
    expect(other && *other == tech::other,
           "Uncommon chemistry strings must map to other");
}

void test_power_source_type_parser() {
    namespace backend = syscape::detail::power_backend;
    using pst = syscape::detail::power_common::power_source_type;

    const auto mains = backend::parse_power_source_type("Mains\n");
    expect(mains && *mains == pst::mains, "Mains must map to mains");

    const auto usb = backend::parse_power_source_type("USB");
    expect(usb && *usb == pst::usb, "USB must map to usb");

    const auto wireless = backend::parse_power_source_type("Wireless");
    expect(wireless && *wireless == pst::wireless,
           "Wireless must map to wireless");

    const auto ups = backend::parse_power_source_type("UPS");
    expect(ups && *ups == pst::ups, "UPS must map to ups");

    const auto other = backend::parse_power_source_type("Solar");
    expect(other && *other == pst::other, "Solar must map to other");

    const auto bat = backend::parse_power_source_type("Battery");
    expect(!bat && bat.error() == syscape::errc::malformed_data,
           "Battery is not an external power source type");

    expect(backend::parse_usb_power_type("USB [PD] DCP") == pst::usb_pd,
           "[PD] must map to usb_pd");
    expect(backend::parse_usb_power_type("USB [PD_DRP]") == pst::usb_pd,
           "[PD_DRP] must map to usb_pd");
    expect(backend::parse_usb_power_type("PD\n") == pst::usb_pd,
           "PD must map to usb_pd");
    expect(backend::parse_usb_power_type("USB [DCP]") == pst::usb,
           "[DCP] must map to usb");
}

void test_temperature_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto tenths = backend::parse_temperature("295\n");
    expect(tenths && *tenths > 29.4 && *tenths < 29.6,
           "295 tenths must parse as 29.5 C");

    const auto hot = backend::parse_temperature("1500");
    expect(hot && *hot > 149.9 && *hot < 150.1,
           "1500 tenths must parse as 150.0 C");

    const auto zero = backend::parse_temperature("0");
    expect(zero && *zero == 0.0, "0 must parse as 0.0 C");

    const auto invalid = backend::parse_temperature("hot");
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Nonnumeric temperature must be malformed");
}

void test_u64_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto num = backend::parse_u64("80000000\n");
    expect(num && *num == 80000000ULL, "80000000 must parse");

    const auto overflow = backend::parse_u64("9999999999999999999999999999");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Overflow must report value_too_large");

    const auto bad = backend::parse_u64("80M");
    expect(!bad && bad.error() == syscape::errc::malformed_data,
           "Trailing text must be malformed");
}

void test_i64_parser() {
    namespace backend = syscape::detail::power_backend;

    const auto pos = backend::parse_i64("15000000\n");
    expect(pos && *pos == 15000000, "15000000 must parse");

    const auto neg = backend::parse_i64("-15000000\n");
    expect(neg && *neg == -15000000, "-15000000 must parse");

    const auto zero = backend::parse_i64("0");
    expect(zero && *zero == 0, "0 must parse");

    const auto bad = backend::parse_i64("-15M");
    expect(!bad && bad.error() == syscape::errc::malformed_data,
           "Trailing text must be malformed");
}

void test_aggregate_compatibility() {
    // Tests that existing aggregate initialization with 1, 3, or 7 fields remains valid.
    const syscape::power::battery_entry single{"BAT0"};
    expect(single.identifier == "BAT0" && single.present &&
               single.state == syscape::power::battery_state::unknown &&
               !single.has_charge_percent,
           "Aggregate initialization with 1 field must succeed");

    const syscape::power::battery_entry triple{
        "BAT0", true, syscape::power::battery_state::charging
    };
    expect(triple.identifier == "BAT0" && triple.present &&
               triple.state == syscape::power::battery_state::charging &&
               !triple.has_charge_percent,
           "Aggregate initialization with 3 fields must succeed");

    const syscape::power::battery_entry legacy{
        "BAT0", true, syscape::power::battery_state::charging,
        true, 80U, false, 0U
    };
    expect(legacy.identifier == "BAT0" && legacy.present &&
               legacy.state == syscape::power::battery_state::charging &&
               legacy.has_charge_percent && legacy.charge_percent == 80U &&
               !legacy.has_cycle_count && legacy.cycle_count == 0U,
           "Aggregate initialization with legacy 7-field order must succeed");
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

    syscape::result<std::vector<common::battery_record>> invalid_mfg(
        std::vector<common::battery_record>{});
    invalid_mfg->push_back(common::battery_record{});
    invalid_mfg->back().identifier = "BAT0";
    invalid_mfg->back().manufacturer = "\xff\xfe";
    const auto mfg_checked =
        common::validate_battery_records(std::move(invalid_mfg));
    expect(!mfg_checked &&
               mfg_checked.error() == syscape::errc::invalid_encoding,
           "Battery manufacturer must be well-formed UTF-8");

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

    syscape::result<std::vector<common::power_source_record>> invalid_ps_enc(
        std::vector<common::power_source_record>{});
    invalid_ps_enc->push_back(common::power_source_record{});
    invalid_ps_enc->back().identifier = "\xff\xfe";
    const auto ps_checked =
        common::validate_power_source_records(std::move(invalid_ps_enc));
    expect(!ps_checked &&
               ps_checked.error() == syscape::errc::invalid_encoding,
           "Power source identifiers must be well-formed UTF-8");
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

    const syscape::result<std::vector<syscape::power::power_source_entry>>
        sources = syscape::power::power_sources();
    expect(sources.has_value(), "Power source enumeration must succeed");
    if (sources) {
        std::string prev_id;
        for (const auto& src : *sources) {
            expect(!src.identifier.empty(),
                   "Power source identifier must not be empty");
            expect(prev_id <= src.identifier,
                   "Power sources must be sorted by identifier");
            prev_id = src.identifier;
        }
    }

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

        if (battery.has_voltage_millivolts) {
            expect(battery.voltage_millivolts > 0U,
                   "Battery voltage must be positive");
        }

        if (battery.has_energy_design_mwh && battery.has_energy_full_mwh) {
            expect(battery.has_health_percent,
                   "Health percent must be computed when energy design and "
                   "full are available");
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

        std::string health_str;
        if (independent_attribute(battery.identifier, "health", health_str)) {
            expect(battery.health != syscape::power::battery_health::unknown ||
                       health_str == "Unknown" || health_str == "No battery",
                   "Reported health should reflect sysfs health attribute");
        }

        std::string tech_str;
        if (independent_attribute(battery.identifier, "technology", tech_str)) {
            expect(battery.technology != syscape::power::battery_technology::unknown ||
                       tech_str == "Unknown",
                   "Reported technology should reflect sysfs technology attribute");
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
    test_aggregate_compatibility();
    test_status_parser();
    test_health_parser();
    test_technology_parser();
    test_power_source_type_parser();
    test_temperature_parser();
    test_u64_parser();
    test_i64_parser();
    test_capacity_parser();
    test_flag_parser();
    test_online_parser();
    test_cycle_count_parser();
    test_type_classifier();
    test_boundary_validation();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
