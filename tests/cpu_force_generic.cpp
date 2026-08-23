#include <cstdint>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/cpu.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::cpu::vendor_identifiers()) &&
                   unsupported(syscape::cpu::model_names()) &&
                   unsupported(syscape::cpu::online_logical_processor_count()) &&
                   unsupported(syscape::cpu::online_physical_core_count()) &&
                   unsupported(syscape::cpu::online_processor_package_count()) &&
                   unsupported(syscape::cpu::minimum_frequency_khz()) &&
                   unsupported(syscape::cpu::maximum_frequency_khz()) &&
                   unsupported(syscape::cpu::current_frequencies_khz()) &&
                   unsupported(syscape::cpu::cache_descriptors()) &&
                   unsupported(syscape::cpu::instruction_set_features()) &&
                   unsupported(syscape::cpu::cumulative_processor_usage())
               ? 0
               : 1;
}
