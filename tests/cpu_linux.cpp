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

} // namespace

int main() {
    test_cpuinfo_parser();
    test_cpu_list_parser();
    test_topology_id_parser();
    test_runtime_queries();
    return failures == 0 ? 0 : 1;
}
