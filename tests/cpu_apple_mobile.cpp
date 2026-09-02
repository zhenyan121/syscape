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
    const auto logical = syscape::cpu::online_logical_processor_count();
    expect(logical && *logical > 0U,
           "logical processor count must be positive");

    const auto physical = syscape::cpu::online_physical_core_count();
    expect(physical && *physical > 0U, "physical core count must be positive");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(packages.has_value() ||
               packages.error() == syscape::errc::not_supported,
           "package count query must succeed or report not_supported");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors.has_value() ||
               vendors.error() == syscape::errc::not_supported,
           "vendor query must succeed or report not_supported");

    const auto models = syscape::cpu::model_names();
    expect(models.has_value() || models.error() == syscape::errc::not_supported,
           "model names query must succeed or report not_supported");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(min_freq.has_value() ||
               min_freq.error() == syscape::errc::not_supported,
           "minimum frequency query must succeed or report not_supported");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(max_freq.has_value() ||
               max_freq.error() == syscape::errc::not_supported,
           "maximum frequency query must succeed or report not_supported");

    const auto cur_freqs = syscape::cpu::current_frequencies_khz();
    expect(cur_freqs.has_value() ||
               cur_freqs.error() == syscape::errc::not_supported,
           "current frequencies query must succeed or report not_supported");

    const auto caches = syscape::cpu::cache_descriptors();
    expect(caches.has_value() || caches.error() == syscape::errc::not_supported,
           "cache descriptors query must succeed or report not_supported");

    const auto features = syscape::cpu::instruction_set_features();
    expect(
        features.has_value() ||
            features.error() == syscape::errc::not_supported,
        "instruction set features query must succeed or report not_supported");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(usage.has_value() || usage.error() == syscape::errc::not_supported,
           "cumulative usage query must succeed or report not_supported");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
