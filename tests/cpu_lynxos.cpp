#include <iostream>

#include <syscape/cpu.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        ++failures;
    }
}

void test_cpu_queries() {
    const auto logical = syscape::cpu::online_logical_processor_count();
#if defined(SYSCAPE_LYNXOS_CPU_COUNT)
    expect(logical.has_value() && *logical == SYSCAPE_LYNXOS_CPU_COUNT,
           "logical processor count must match SYSCAPE_LYNXOS_CPU_COUNT");
#elif defined(LYNXOS_CPU_COUNT)
    expect(logical.has_value() && *logical == LYNXOS_CPU_COUNT,
           "logical processor count must match LYNXOS_CPU_COUNT");
#else
    // On POSIX host checks, sysconf(_SC_NPROCESSORS_ONLN) or not_supported
    if (logical.has_value()) {
        expect(*logical > 0, "logical processor count must be positive");
    } else {
        expect(logical.error() == syscape::errc::not_supported,
               "logical processor count failure must report not_supported");
    }
#endif

    const auto phys = syscape::cpu::online_physical_core_count();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical core count must report not_supported on LynxOS");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(!packages && packages.error() == syscape::errc::not_supported,
           "package count must report not_supported on LynxOS");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(!min_freq && min_freq.error() == syscape::errc::not_supported,
           "minimum frequency must report not_supported on LynxOS");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(!max_freq && max_freq.error() == syscape::errc::not_supported,
           "maximum frequency must report not_supported on LynxOS");

    const auto cur_freq = syscape::cpu::current_frequencies_khz();
    expect(!cur_freq && cur_freq.error() == syscape::errc::not_supported,
           "current frequencies must report not_supported on LynxOS");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(!vendors && vendors.error() == syscape::errc::not_supported,
           "vendor identifiers must report not_supported on LynxOS");

    const auto models = syscape::cpu::model_names();
    expect(!models && models.error() == syscape::errc::not_supported,
           "model names must report not_supported on LynxOS");

    const auto caches = syscape::cpu::cache_descriptors();
    expect(!caches && caches.error() == syscape::errc::not_supported,
           "cache descriptors must report not_supported on LynxOS");

    const auto features = syscape::cpu::instruction_set_features();
    expect(!features && features.error() == syscape::errc::not_supported,
           "instruction set features must report not_supported on LynxOS");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(!usage && usage.error() == syscape::errc::not_supported,
           "cumulative processor usage must report not_supported on LynxOS");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
