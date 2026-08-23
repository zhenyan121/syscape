#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/linux.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpuinfo_parser() {
    const std::string input =
        "processor : 0\n"
        "vendor_id : Vendor A\n"
        "model name : Model One\n"
        "\n"
        "processor : 1\n"
        "vendor_id : Vendor A\n"
        "model name : Model Two\n"
        "\n"
        "processor : 2\n"
        "CPU implementer : 0x41\n"
        "Processor : Model Two\n";
    const auto parsed = syscape::detail::cpu_backend::parse_cpuinfo(input);
    expect(parsed && parsed->vendors.size() == 2U,
           "CPU vendors must be distinct and complete");
    expect(parsed && parsed->vendors[0] == "Vendor A" &&
               parsed->vendors[1] == "0x41",
           "CPU vendor order must follow the platform source");
    expect(parsed && parsed->models.size() == 2U &&
               parsed->models[0] == "Model One" &&
               parsed->models[1] == "Model Two",
           "CPU models must be distinct and complete");

    const auto malformed =
        syscape::detail::cpu_backend::parse_cpuinfo("vendor_id :   \n");
    expect(!malformed && malformed.error() == syscape::errc::malformed_data,
           "An empty recognized CPU label must be malformed");

    std::vector<std::string> invalid_labels;
    invalid_labels.emplace_back(1U, static_cast<char>(0xff));
    const auto invalid_utf8 = syscape::detail::cpu_common::validate_utf8_labels(
        syscape::result<std::vector<std::string>>(invalid_labels));
    expect(!invalid_utf8 &&
               invalid_utf8.error() == syscape::errc::malformed_data,
           "Invalid CPU label encoding must fail at the public boundary");
}

void test_cpu_list_parser() {
    const auto parsed =
        syscape::detail::cpu_backend::parse_cpu_list("0-2,4,7-8\n");
    const std::vector<std::uint32_t> expected{0U, 1U, 2U, 4U, 7U, 8U};
    expect(parsed && *parsed == expected, "Linux CPU ranges must be expanded");

    expect(!syscape::detail::cpu_backend::parse_cpu_list(""),
           "An empty CPU list must fail");
    expect(!syscape::detail::cpu_backend::parse_cpu_list("3-1"),
           "A descending CPU range must fail");
    expect(!syscape::detail::cpu_backend::parse_cpu_list("1,,2"),
           "An empty CPU-list item must fail");
    expect(!syscape::detail::cpu_backend::parse_cpu_list("1-x"),
           "A nonnumeric CPU range must fail");
}

void test_frequency_parsers() {
    using syscape::detail::cpu_backend::parse_khz_text;
    using syscape::detail::cpu_backend::parse_mhz_text;

    const auto kilohertz = parse_khz_text("123456\n");
    expect(kilohertz && *kilohertz == 123456U,
           "A sysfs frequency must parse exactly");
    expect(parse_khz_text("  789  ") && *parse_khz_text("  789  ") == 789U,
           "Surrounding whitespace in a frequency record must be tolerated");
    expect(!parse_khz_text(""),
           "An empty frequency record must be malformed");
    expect(!parse_khz_text("0"),
           "A zero clock cannot describe an operating processor");
    expect(!parse_khz_text("-5"),
           "A negative frequency record must be malformed");
    expect(!parse_khz_text("12x"),
           "Trailing junk after a frequency must be malformed");
    expect(!parse_khz_text("99999999999"),
           "An unrepresentable frequency must report value_too_large");
    expect(parse_khz_text("99999999999").error() ==
               syscape::errc::value_too_large,
           "Frequency overflow must use the value_too_large condition");
    expect(parse_khz_text("18446744073709551616").error() ==
               syscape::errc::value_too_large,
           "Frequency text beyond uint64_t must remain an overflow");

    const auto megahertz = parse_mhz_text("800.011");
    expect(megahertz && *megahertz == 800011U,
           "A cpu MHz value must convert exactly at millihertz precision");
    const auto integral = parse_mhz_text("2400");
    expect(integral && *integral == 2400000U,
           "An integral cpu MHz value must scale by one thousand");
    const auto rounded = parse_mhz_text("1234.5678");
    expect(rounded && *rounded == 1234568U,
           "A fourth fractional digit of five or more must round up");
    expect(parse_mhz_text("1234.5674") &&
               *parse_mhz_text("1234.5674") == 1234567U,
           "A fourth fractional digit below five must not round up");
    expect(!parse_mhz_text(".5"), "MHz text without an integer part must fail");
    expect(!parse_mhz_text("12."), "MHz text without fraction digits must fail");
    expect(!parse_mhz_text("1e3"), "Non-decimal MHz renderings must fail");
    expect(!parse_mhz_text("1.2.3"), "Repeated decimal points must fail");
    expect(!parse_mhz_text(""), "Empty MHz text must be malformed");
}

void test_cpuinfo_frequencies_parser() {
    using syscape::detail::cpu_backend::parse_cpuinfo_frequencies;

    const std::string input =
        "processor : 0\n"
        "vendor_id : Vendor A\n"
        "cpu MHz : 800.011\n"
        "\n"
        "processor : 1\n"
        "cpu MHz\t\t: 2400.000\n";
    const auto parsed = parse_cpuinfo_frequencies(input);
    expect(parsed && parsed->size() == 2U && (*parsed)[0] == 800011U &&
               (*parsed)[1] == 2400000U,
           "Per-processor MHz fields must convert in platform order");

    const std::string arm_style =
        "processor : 0\n"
        "CPU implementer : 0x41\n";
    const auto without_fields = parse_cpuinfo_frequencies(arm_style);
    expect(!without_fields &&
               without_fields.error() == syscape::errc::not_supported,
           "cpuinfo blocks without any MHz field must mean not supported");

    const std::string partial =
        "processor : 0\n"
        "cpu MHz : 800.011\n"
        "\n"
        "processor : 1\n";
    const auto incomplete = parse_cpuinfo_frequencies(partial);
    expect(!incomplete &&
               incomplete.error() == syscape::errc::malformed_data,
           "Partially covered processor blocks must be malformed");

    const auto empty = parse_cpuinfo_frequencies("");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "Empty cpuinfo text must be malformed");

    const auto orphan = parse_cpuinfo_frequencies(
        std::string("cpu MHz : 800.011\n"));
    expect(!orphan && orphan.error() == syscape::errc::malformed_data,
           "A MHz field outside a processor block must be malformed");

    const auto bad_index = parse_cpuinfo_frequencies(
        std::string("processor : x\n") + "cpu MHz : 800.011\n");
    expect(!bad_index && bad_index.error() == syscape::errc::malformed_data,
           "A nonnumeric processor index must be malformed");
}

void test_proc_stat_usage_parser() {
    namespace backend = syscape::detail::cpu_backend;

    const auto full = backend::parse_proc_stat_usage(
        "cpu  100 10 30 400 50 6 7 8 9 10\n"
        "cpu0 50 5 15 200 25 3 3 4 4 5\n"
        "intr 123\n");
    expect(full && full->user_ticks == 110U && full->system_ticks == 43U &&
               full->idle_ticks == 450U,
           "Aggregate usage must fold user+nice, system+irq+softirq, and "
           "idle+iowait while excluding steal");

    const auto minimal = backend::parse_proc_stat_usage(
        "cpu  100 10 30 400\n");
    expect(minimal && minimal->user_ticks == 110U &&
               minimal->system_ticks == 30U && minimal->idle_ticks == 400U,
           "The four-field historical aggregate line must still fold");

    const auto malformed_fields = backend::parse_proc_stat_usage(
        "cpu  100 10 30\n");
    expect(!malformed_fields &&
               malformed_fields.error() == syscape::errc::malformed_data,
           "An aggregate line with fewer than four counters must be "
           "malformed");

    const auto nonnumeric = backend::parse_proc_stat_usage(
        "cpu  100 ten 30 400 50 6 7 8 9 10\n");
    expect(!nonnumeric &&
               nonnumeric.error() == syscape::errc::malformed_data,
           "A nonnumeric counter must be malformed");

    const auto negative = backend::parse_proc_stat_usage(
        "cpu  -1 10 30 400\n");
    expect(!negative && negative.error() == syscape::errc::malformed_data,
           "A negative counter must be malformed");

    const auto oversized = backend::parse_proc_stat_usage(
        "cpu  18446744073709551616 10 30 400\n");
    expect(!oversized &&
               oversized.error() == syscape::errc::value_too_large,
           "A stat counter beyond uint64_t must report value_too_large");

    expect(!backend::parse_proc_stat_usage("intr 123\n"),
           "Stat text without an aggregate cpu line must report not_found");

    const auto per_cpu_only = backend::parse_proc_stat_usage(
        "cpu0 50 5 15 200 25 3 3 4 4 5\n");
    expect(!per_cpu_only &&
               per_cpu_only.error() == syscape::errc::not_found,
           "Per-processor lines must never satisfy the aggregate query");
}

void test_topology_id_parser() {
    const auto value = syscape::detail::cpu_backend::parse_topology_id("42\n");
    expect(value && *value == 42, "A topology ID must parse exactly");

    const auto unavailable =
        syscape::detail::cpu_backend::parse_topology_id("-1\n");
    expect(!unavailable && unavailable.error() == syscape::errc::not_supported,
           "The kernel unknown topology ID must mean not supported");

    const auto malformed =
        syscape::detail::cpu_backend::parse_topology_id("-2\n");
    expect(!malformed && malformed.error() == syscape::errc::malformed_data,
           "Other negative topology IDs must be malformed");
}

void test_runtime_queries() {
    const auto logical = syscape::cpu::online_logical_processor_count();
    expect(logical && *logical > 0U,
           "Linux must report a positive online logical processor count");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors || vendors.error() == syscape::errc::not_found,
           "CPU vendor identifiers must succeed or report not_found");
    if (vendors) {
        for (const std::string& value : *vendors) {
            expect(!value.empty() && syscape::detail::is_valid_utf8(value),
                   "CPU vendor identifiers must be nonempty UTF-8");
        }
    }

    const auto models = syscape::cpu::model_names();
    expect(models || models.error() == syscape::errc::not_found,
           "CPU model labels must succeed or report not_found");
    if (models) {
        for (const std::string& value : *models) {
            expect(!value.empty() && syscape::detail::is_valid_utf8(value),
                   "CPU model labels must be nonempty UTF-8");
        }
    }

    const auto physical = syscape::cpu::online_physical_core_count();
    const auto packages = syscape::cpu::online_processor_package_count();
    if (physical && packages && logical) {
        expect(*packages <= *physical && *physical <= *logical,
               "Package, physical-core, and logical counts must be ordered");
    } else {
        expect(!physical || !packages,
               "Physical topology queries must fail consistently");
    }
}

void test_runtime_frequencies() {
    // The frequency query and the count queries are independent reads of
    // system state; a hot-plug event between them legitimately changes the
    // online population, so matching either observation is acceptable.
    const auto logical_before = syscape::cpu::online_logical_processor_count();
    const auto current = syscape::cpu::current_frequencies_khz();
    const auto logical_after = syscape::cpu::online_logical_processor_count();
    if (current) {
        expect(!current->empty(),
               "A successful frequency query must list at least one clock");
        for (const std::uint32_t value : *current) {
            expect(value > 0U, "Reported clocks must be positive");
        }
        expect(logical_before && logical_after &&
                   (current->size() == *logical_before ||
                    current->size() == *logical_after),
               "Frequency entries must cover the online logical processors "
               "observed around the query");
    } else {
        expect(current.error() == syscape::errc::not_supported ||
                   current.error() == syscape::errc::not_found ||
                   current.error() == syscape::errc::malformed_data,
               "Linux frequency failures must stay within documented "
               "conditions");
    }

    const auto minimum = syscape::cpu::minimum_frequency_khz();
    const auto maximum = syscape::cpu::maximum_frequency_khz();
    expect(static_cast<bool>(minimum) == static_cast<bool>(maximum),
           "Recorded clock bounds must be available together on Linux");
    if (minimum && maximum) {
        expect(*minimum <= *maximum,
               "The recorded lower bound cannot exceed the upper bound");
    }
}

void test_cache_attribute_parsers() {
    using syscape::detail::cpu_backend::processor_sets_overlap;
    using syscape::detail::cpu_backend::same_cache_geometry;
    using syscape::detail::cpu_backend::parse_cache_size_bytes;
    using syscape::detail::cpu_backend::parse_cache_type;
    using syscape::detail::cpu_backend::parse_optional_cache_attribute;
    using syscape::detail::cpu_backend::parse_positive_cache_attribute;

    const auto kibibytes = parse_cache_size_bytes("32K\n");
    expect(kibibytes && *kibibytes == 32768U,
           "A documented kibibyte cache size must convert exactly");
    expect(parse_cache_size_bytes("1024K") &&
               *parse_cache_size_bytes("1024K") == 1048576U,
           "A larger kibibyte rendering must stay exact");
    expect(parse_cache_size_bytes("2M") && *parse_cache_size_bytes("2M") ==
               2097152U,
           "A mebibyte rendering must be accepted defensively");
    expect(!parse_cache_size_bytes(""), "Empty size text must fail");
    expect(!parse_cache_size_bytes("32"),
           "A size without a suffix must be malformed");
    expect(!parse_cache_size_bytes("32X"),
           "An unknown suffix must be malformed");
    expect(!parse_cache_size_bytes("-32K"),
           "A negative size must be malformed");
    expect(!parse_cache_size_bytes("0K"),
           "A zero size cannot describe real hardware");
    expect(!parse_cache_size_bytes("1.5M"),
           "A fractional size rendering must be malformed");
    expect(!parse_cache_size_bytes("18446744073709551616K"),
           "An unrepresentable digit part must report value_too_large");
    expect(parse_cache_size_bytes("18446744073709551616K").error() ==
               syscape::errc::value_too_large,
           "Cache size overflow must use the value_too_large condition");
    expect(!parse_cache_size_bytes("17592186044416G"),
           "A gigabyte rendering beyond uint64_t bytes must overflow");

    const auto data_kind = parse_cache_type("Data\n");
    expect(data_kind &&
               *data_kind == syscape::cpu::cache_kind::data,
           "The documented Data rendering must map to the data kind");
    expect(parse_cache_type("Instruction") &&
               *parse_cache_type("Instruction") ==
                   syscape::cpu::cache_kind::instruction,
           "The documented Instruction rendering must map exactly");
    expect(parse_cache_type("Unified") &&
               *parse_cache_type("Unified") ==
                   syscape::cpu::cache_kind::unified,
           "The documented Unified rendering must map exactly");
    expect(!parse_cache_type("Trace"),
           "A type the Linux source never renders must be malformed");
    expect(!parse_cache_type("data"), "Case must match the ABI exactly");
    expect(!parse_cache_type(""), "An empty type rendering must fail");

    const auto level = parse_positive_cache_attribute("1\n");
    expect(level && *level == 1U, "A positive attribute must parse exactly");
    const auto line = parse_positive_cache_attribute("64");
    expect(line && *line == 64U, "Line sizes must parse exactly");
    expect(!parse_positive_cache_attribute("0"),
           "Zero cannot describe a level or line size");
    expect(!parse_positive_cache_attribute("-1"),
           "The unknown marker cannot describe a required quantity");
    expect(!parse_positive_cache_attribute("x"),
           "Nonnumeric attributes must be malformed");
    expect(!parse_positive_cache_attribute("99999999999"),
           "Attributes beyond uint32_t must report value_too_large");

    const auto ways = parse_optional_cache_attribute("16\n");
    expect(ways && *ways == 16U, "Optional geometry must parse exactly");
    expect(parse_optional_cache_attribute("") &&
               *parse_optional_cache_attribute("") == 0U,
           "An absent optional attribute must record not reported");
    expect(parse_optional_cache_attribute("-1\n") &&
               *parse_optional_cache_attribute("-1\n") == 0U,
           "The kernel unknown marker must record not reported");
    expect(!parse_optional_cache_attribute("-2"),
           "Other negative geometry renderings must be malformed");

    syscape::detail::cpu_common::cache_entry first;
    first.level = 1U;
    first.kind = syscape::cpu::cache_kind::data;
    first.instance_size_bytes = 32768U;
    first.line_size_bytes = 64U;
    first.associativity_ways = 8U;
    first.sets_count = 64U;
    first.shared_logical_processor_count = 2U;
    auto second = first;
    expect(same_cache_geometry(first, second),
           "Repeated observations must accept identical geometry");
    second.instance_size_bytes *= 2U;
    expect(!same_cache_geometry(first, second),
           "Repeated observations must reject contradictory geometry");
    expect(processor_sets_overlap({0U, 2U}, {1U, 2U}),
           "Sharing-set overlap must be detected");
    expect(!processor_sets_overlap({0U, 2U}, {1U, 3U}),
           "Disjoint sharing sets must remain distinct");
}

void test_cpuinfo_features_parser() {
    using syscape::detail::cpu_backend::parse_cpuinfo_features;

    const std::string x86_style =
        "processor : 0\n"
        "vendor_id : Vendor A\n"
        "flags : fpu sse sse2\n"
        "\n"
        "processor : 1\n"
        "flags\t\t: fpu avx2 sse2\n";
    const auto merged = parse_cpuinfo_features(x86_style);
    const std::vector<std::string> expected{"fpu", "sse", "sse2", "avx2"};
    expect(merged && *merged == expected,
           "Feature tokens must union across blocks in first-seen order");

    const std::string arm_style =
        "processor : 0\n"
        "CPU implementer : 0x41\n"
        "Features : asimd aes pmull\n";
    const auto single = parse_cpuinfo_features(arm_style);
    expect(single && single->size() == 3U && (*single)[0] == "asimd",
           "The arm Features rendering must supply its tokens verbatim");

    const auto without_fields = parse_cpuinfo_features(
        std::string("processor : 0\nisa : rv64imafdch\n"));
    expect(!without_fields &&
               without_fields.error() == syscape::errc::not_found,
           "Blocks without any recognized feature field must mean "
           "not found");

    const auto partial = parse_cpuinfo_features(
        "processor : 0\nflags : fpu\n\nprocessor : 1\nvendor_id : V\n");
    expect(!partial && partial.error() == syscape::errc::malformed_data,
           "Partially covered processor blocks must be malformed");

    const auto empty_value = parse_cpuinfo_features(
        std::string("processor : 0\nflags :   \n"));
    expect(!empty_value && empty_value.error() == syscape::errc::malformed_data,
           "An empty recognized rendering must be malformed");

    const auto orphan = parse_cpuinfo_features(std::string("flags : fpu\n"));
    expect(!orphan && orphan.error() == syscape::errc::malformed_data,
           "A feature field outside a processor block must be malformed");

    const auto bad_index = parse_cpuinfo_features(
        std::string("processor : x\nflags : fpu\n"));
    expect(!bad_index && bad_index.error() == syscape::errc::malformed_data,
           "A nonnumeric processor index must be malformed");

    const auto empty_text = parse_cpuinfo_features("");
    expect(!empty_text && empty_text.error() == syscape::errc::malformed_data,
           "Empty cpuinfo text must be malformed");

    const auto no_trailing_newline = parse_cpuinfo_features(
        std::string("processor : 0\nflags : fpu"));
    expect(no_trailing_newline && no_trailing_newline->size() == 1U,
           "A final block without a newline must still flush");
}

void test_runtime_caches() {
    const auto caches = syscape::cpu::cache_descriptors();
    if (!caches) {
        expect(caches.error() == syscape::errc::not_supported ||
                   caches.error() == syscape::errc::malformed_data ||
                   caches.error() == syscape::errc::temporarily_unavailable,
               "Linux cache failures must stay within documented "
               "conditions");
        return;
    }
    expect(!caches->empty(),
           "A successful cache query must list at least one instance");
    bool ordered = true;
    for (std::size_t position = 1U; position < caches->size(); ++position) {
        const auto& previous = (*caches)[position - 1U];
        const auto& current = (*caches)[position];
        if (current.level < previous.level ||
            (current.level == previous.level &&
             current.kind < previous.kind)) {
            ordered = false;
        }
    }
    expect(ordered, "Cache entries must follow the documented order");
    for (const auto& entry : *caches) {
        expect(entry.level > 0U, "Cache levels start at one");
        expect(entry.instance_size_bytes > 0U, "Instance sizes are positive");
        expect(entry.line_size_bytes > 0U, "Line sizes are positive");
    }

    // Every online logical processor owns exactly one level-one data
    // instance, so their sharing counts must cover the online population.
    const auto logical = syscape::cpu::online_logical_processor_count();
    std::uint64_t level_one_data_shares = 0U;
    for (const auto& entry : *caches) {
        if (entry.level == 1U &&
            entry.kind == syscape::cpu::cache_kind::data) {
            level_one_data_shares += entry.shared_logical_processor_count;
        }
    }
    if (logical && level_one_data_shares > 0U) {
        expect(level_one_data_shares == static_cast<std::uint64_t>(*logical),
               "Level-one data sharing counts must cover every online "
               "processor exactly once");
    }
}

void test_runtime_features() {
    const auto features = syscape::cpu::instruction_set_features();
    expect(features || features.error() == syscape::errc::not_found ||
               features.error() == syscape::errc::not_supported,
           "Feature failures must stay within documented conditions");
    if (!features) { return; }

    std::vector<std::string> unique_check;
    for (const std::string& identifier : *features) {
        expect(!identifier.empty(), "Identifiers must be nonempty");
        expect(syscape::detail::is_valid_utf8(identifier),
               "Identifiers must be valid UTF-8");
        expect(std::find(unique_check.begin(), unique_check.end(),
                         identifier) == unique_check.end(),
               "Feature identifiers must be unique");
        unique_check.push_back(identifier);
    }
}

void test_runtime_usage() {
    const auto first = syscape::cpu::cumulative_processor_usage();
    if (!first) {
        expect(first.error() == syscape::errc::not_supported ||
                   first.error() == syscape::errc::not_found ||
                   first.error() == syscape::errc::malformed_data,
               "Usage failures must stay within documented conditions");
        return;
    }
    const auto second = syscape::cpu::cumulative_processor_usage();
    expect(second.has_value(),
           "A successful usage query must repeat successfully");
    if (!second) { return; }
    expect(second->user_ticks >= first->user_ticks &&
               second->system_ticks >= first->system_ticks &&
               second->idle_ticks >= first->idle_ticks,
           "Cumulative counters must never decrease between calls");
}

} // namespace

int main() {
    test_cpuinfo_parser();
    test_cpu_list_parser();
    test_topology_id_parser();
    test_frequency_parsers();
    test_cpuinfo_frequencies_parser();
    test_cache_attribute_parsers();
    test_cpuinfo_features_parser();
    test_proc_stat_usage_parser();
    test_runtime_queries();
    test_runtime_frequencies();
    test_runtime_caches();
    test_runtime_features();
    test_runtime_usage();
    return failures == 0 ? 0 : 1;
}
