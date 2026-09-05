#include <iostream>
#include <string>
#include <vector>

#include <syscape/cpu.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpu_queries() {
    const auto count = syscape::cpu::online_logical_processor_count();
    expect(count && *count > 0, "logical core count must be positive");

    const auto models = syscape::cpu::model_names();
    expect(
        models.has_value() || models.error() == syscape::errc::not_supported,
        "model names query must succeed or report not_supported on GNU/Hurd");
    if (models) {
        for (std::size_t i = 0; i < models->size(); ++i) {
            for (std::size_t j = i + 1; j < models->size(); ++j) {
                expect((*models)[i] != (*models)[j],
                       "model names must be deduplicated");
            }
        }
    }

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors.has_value() ||
               vendors.error() == syscape::errc::not_supported,
           "vendor identifiers query must succeed or report not_supported on "
           "GNU/Hurd");
    if (vendors) {
        for (std::size_t i = 0; i < vendors->size(); ++i) {
            for (std::size_t j = i + 1; j < vendors->size(); ++j) {
                expect((*vendors)[i] != (*vendors)[j],
                       "vendor identifiers must be deduplicated");
            }
        }
    }

    const auto phys_cores = syscape::cpu::online_physical_core_count();
    expect(phys_cores.error() == syscape::errc::not_supported,
           "physical core count query must report not_supported on GNU/Hurd");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(packages.error() == syscape::errc::not_supported,
           "package count query must report not_supported on GNU/Hurd");

    const auto freqs = syscape::cpu::current_frequencies_khz();
    expect(freqs.error() == syscape::errc::not_supported,
           "current frequencies query must report not_supported on GNU/Hurd");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(min_freq.error() == syscape::errc::not_supported,
           "minimum frequency query must report not_supported on GNU/Hurd");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(max_freq.error() == syscape::errc::not_supported,
           "maximum frequency query must report not_supported on GNU/Hurd");

    const auto isas = syscape::cpu::instruction_set_features();
    expect(isas.error() == syscape::errc::not_supported,
           "instruction set query must report not_supported on GNU/Hurd");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(usage.has_value() || usage.error() == syscape::errc::not_supported,
           "cumulative processor usage query must succeed or report "
           "not_supported on GNU/Hurd");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
