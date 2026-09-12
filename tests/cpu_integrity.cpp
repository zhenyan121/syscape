#include <iostream>

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
    expect(count.has_value() && *count == 4U,
           "logical processor count must match mock GetProcessorCount on "
           "INTEGRITY");

    const auto phys = syscape::cpu::online_physical_core_count();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical core count must report not_supported on INTEGRITY");

    const auto pkg = syscape::cpu::online_processor_package_count();
    expect(!pkg && pkg.error() == syscape::errc::not_supported,
           "package count must report not_supported on INTEGRITY");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(!vendors && vendors.error() == syscape::errc::not_supported,
           "vendors must report not_supported on INTEGRITY");

    const auto models = syscape::cpu::model_names();
    expect(!models && models.error() == syscape::errc::not_supported,
           "models must report not_supported on INTEGRITY");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(!min_freq && min_freq.error() == syscape::errc::not_supported,
           "minimum frequency must report not_supported on INTEGRITY");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(!max_freq && max_freq.error() == syscape::errc::not_supported,
           "maximum frequency must report not_supported on INTEGRITY");

    const auto cur_freq = syscape::cpu::current_frequencies_khz();
    expect(!cur_freq && cur_freq.error() == syscape::errc::not_supported,
           "current frequencies must report not_supported on INTEGRITY");

    const auto caches = syscape::cpu::cache_descriptors();
    expect(!caches && caches.error() == syscape::errc::not_supported,
           "caches must report not_supported on INTEGRITY");

    const auto isas = syscape::cpu::instruction_set_features();
    expect(!isas && isas.error() == syscape::errc::not_supported,
           "instruction set features must report not_supported on INTEGRITY");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(!usage && usage.error() == syscape::errc::not_supported,
           "cumulative usage must report not_supported on INTEGRITY");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
