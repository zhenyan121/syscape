#include <iostream>

#include <p4.h>

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
#if defined(SYSCAPE_PIKEOS_CPU_COUNT)
    expect(logical.has_value() && *logical == SYSCAPE_PIKEOS_CPU_COUNT,
           "logical processor count must match SYSCAPE_PIKEOS_CPU_COUNT");
#elif defined(PIKEOS_CPU_COUNT)
    expect(logical.has_value() && *logical == PIKEOS_CPU_COUNT,
           "logical processor count must match PIKEOS_CPU_COUNT");
#elif defined(P4_CPU_COUNT)
    expect(logical.has_value() && *logical == P4_CPU_COUNT,
           "logical processor count must match P4_CPU_COUNT");
#else
    expect(!logical && logical.error() == syscape::errc::not_supported,
           "logical processor count must report not_supported on PikeOS by "
           "default");
#endif

    const auto phys = syscape::cpu::online_physical_core_count();
    expect(!phys && phys.error() == syscape::errc::not_supported,
           "physical core count must report not_supported on PikeOS");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(!packages && packages.error() == syscape::errc::not_supported,
           "package count must report not_supported on PikeOS");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(!min_freq && min_freq.error() == syscape::errc::not_supported,
           "minimum frequency must report not_supported on PikeOS");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(!max_freq && max_freq.error() == syscape::errc::not_supported,
           "maximum frequency must report not_supported on PikeOS");

    const auto cur_freq = syscape::cpu::current_frequencies_khz();
    expect(!cur_freq && cur_freq.error() == syscape::errc::not_supported,
           "current frequencies must report not_supported on PikeOS");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(!vendors && vendors.error() == syscape::errc::not_supported,
           "vendor identifiers must report not_supported on PikeOS");

    const auto models = syscape::cpu::model_names();
    expect(!models && models.error() == syscape::errc::not_supported,
           "model names must report not_supported on PikeOS");

    const auto caches = syscape::cpu::cache_descriptors();
    expect(!caches && caches.error() == syscape::errc::not_supported,
           "cache descriptors must report not_supported on PikeOS");

    const auto features = syscape::cpu::instruction_set_features();
    expect(!features && features.error() == syscape::errc::not_supported,
           "instruction set features must report not_supported on PikeOS");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(!usage && usage.error() == syscape::errc::not_supported,
           "cumulative processor usage must report not_supported on PikeOS");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
