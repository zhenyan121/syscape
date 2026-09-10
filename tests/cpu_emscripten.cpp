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
    expect(!count && count.error() == syscape::errc::not_supported,
           "online logical processor count must report not_supported on "
           "Emscripten");

    const auto physical = syscape::cpu::online_physical_core_count();
    expect(!physical && physical.error() == syscape::errc::not_supported,
           "online physical core count must report not_supported");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(!packages && packages.error() == syscape::errc::not_supported,
           "online processor package count must report not_supported");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(!vendors && vendors.error() == syscape::errc::not_supported,
           "vendor identifiers must report not_supported");

    const auto models = syscape::cpu::model_names();
    expect(!models && models.error() == syscape::errc::not_supported,
           "model names must report not_supported");

    const auto features = syscape::cpu::instruction_set_features();
    expect(!features && features.error() == syscape::errc::not_supported,
           "instruction set features must report not_supported");

    const auto caches = syscape::cpu::cache_descriptors();
    expect(!caches && caches.error() == syscape::errc::not_supported,
           "cache descriptors must report not_supported");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(!usage && usage.error() == syscape::errc::not_supported,
           "cumulative processor usage must report not_supported");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
