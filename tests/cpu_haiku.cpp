#include <iostream>
#include <string>

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
    expect(logical && *logical > 0, "logical processor count must be positive");
    const auto physical = syscape::cpu::online_physical_core_count();
    expect((physical && *physical > 0) ||
               physical.error() == syscape::errc::not_supported,
           "physical core count must be positive or not_supported");
    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(!min_freq && min_freq.error() == syscape::errc::not_supported,
           "minimum frequency must report not_supported on Haiku");
    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(!max_freq && max_freq.error() == syscape::errc::not_supported,
           "maximum frequency must report not_supported on Haiku");
    const auto cur_freqs = syscape::cpu::current_frequencies_khz();
    expect(cur_freqs.has_value() ||
               cur_freqs.error() == syscape::errc::not_supported ||
               cur_freqs.error() == syscape::errc::malformed_data,
           "current frequencies must succeed, report not_supported, or "
           "malformed_data");
    if (cur_freqs) {
        const auto fresh_logical =
            syscape::cpu::online_logical_processor_count();
        if (fresh_logical) {
            expect(
                cur_freqs->size() == *fresh_logical,
                "current frequencies count must equal online logical processor "
                "count");
        }
        for (const auto f : *cur_freqs) {
            expect(f > 0, "frequency must be positive");
        }
    }
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
