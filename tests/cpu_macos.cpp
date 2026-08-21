#include <system_error>

#include <syscape/cpu.hpp>

int main() {
    const auto logical = syscape::cpu::online_logical_processor_count();
    const auto physical = syscape::cpu::online_physical_core_count();
    if (!logical || !physical || *logical == 0U || *physical == 0U ||
        *physical > *logical) {
        return 1;
    }
    if (syscape::cpu::online_processor_package_count().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::vendor_identifiers().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::model_names().error() !=
            std::errc::operation_not_supported) {
        return 2;
    }
    return 0;
}
