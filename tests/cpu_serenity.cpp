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
    expect(count.error() == syscape::errc::not_supported,
           "logical core count must report not_supported on SerenityOS");

    const auto models = syscape::cpu::model_names();
    expect(models.error() == syscape::errc::not_supported,
           "model names must report not_supported on SerenityOS");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors.error() == syscape::errc::not_supported,
           "vendor identifiers must report not_supported on SerenityOS");

    const auto phys_cores = syscape::cpu::online_physical_core_count();
    expect(phys_cores.error() == syscape::errc::not_supported,
           "physical core count query must report not_supported on SerenityOS");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(packages.error() == syscape::errc::not_supported,
           "package count query must report not_supported on SerenityOS");

    const auto freqs = syscape::cpu::current_frequencies_khz();
    expect(freqs.error() == syscape::errc::not_supported,
           "current frequencies query must report not_supported on SerenityOS");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(min_freq.error() == syscape::errc::not_supported,
           "minimum frequency query must report not_supported on SerenityOS");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(max_freq.error() == syscape::errc::not_supported,
           "maximum frequency query must report not_supported on SerenityOS");

    const auto isas = syscape::cpu::instruction_set_features();
    expect(isas.error() == syscape::errc::not_supported,
           "instruction set query must report not_supported on SerenityOS");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(usage.error() == syscape::errc::not_supported,
           "cumulative processor usage must report not_supported on "
           "SerenityOS");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
