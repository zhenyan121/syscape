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
                   unsupported(syscape::cpu::online_processor_package_count())
               ? 0
               : 1;
}
