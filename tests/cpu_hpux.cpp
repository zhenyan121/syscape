#include <iostream>
#include <cstdlib>
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
    expect(models.error() == syscape::errc::not_supported,
           "model names query must report not_supported on HP-UX");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors.error() == syscape::errc::not_supported,
           "vendor identifiers query must report not_supported on HP-UX");

    const auto phys_cores = syscape::cpu::online_physical_core_count();
    expect(phys_cores.error() == syscape::errc::not_supported,
           "physical core count query must report not_supported on HP-UX");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(packages.error() == syscape::errc::not_supported,
           "package count query must report not_supported on HP-UX");

    const auto freqs = syscape::cpu::current_frequencies_khz();
    expect(freqs.error() == syscape::errc::not_supported,
           "current frequencies query must report not_supported on HP-UX");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(min_freq.error() == syscape::errc::not_supported,
           "minimum frequency query must report not_supported on HP-UX");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(max_freq.error() == syscape::errc::not_supported,
           "maximum frequency query must report not_supported on HP-UX");

    const auto isas = syscape::cpu::instruction_set_features();
    expect(isas.error() == syscape::errc::not_supported,
           "instruction set query must report not_supported on HP-UX");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(usage.has_value(),
           "cumulative processor usage query must succeed with pstat");
    if (usage) {
        const auto total_ticks =
            usage->user_ticks + usage->system_ticks + usage->idle_ticks;
        expect(total_ticks > 0, "cumulative processor ticks must be non-zero");
        expect(usage->user_ticks > 0, "user ticks must be non-zero");
        expect(usage->system_ticks > 0, "system ticks must be non-zero");
        expect(usage->idle_ticks > 0, "idle ticks must be non-zero");
        const std::uint64_t enabled_count = usage->user_ticks / 550U;
        expect(enabled_count > 0 && usage->user_ticks == enabled_count * 550U,
               "disabled processors must not contribute user ticks");
        expect(usage->system_ticks == enabled_count * 275U,
               "disabled processors must not contribute system ticks");
        expect(usage->idle_ticks == enabled_count * 4100U,
               "wait ticks must be included in idle ticks");
    }

#if defined(SYSCAPE_HPUX_PSTAT_MOCK)
    ::setenv("SYSCAPE_TEST_PSTAT_PROCESSOR_ZERO", "1", 1);
    const auto unavailable_usage = syscape::cpu::cumulative_processor_usage();
    expect(unavailable_usage.error() == syscape::errc::temporarily_unavailable,
           "an empty processor snapshot must be temporarily unavailable");
    ::unsetenv("SYSCAPE_TEST_PSTAT_PROCESSOR_ZERO");
#endif
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
