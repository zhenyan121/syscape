#include <cstdint>
#include <cstring>
#include <system_error>

#include <windows.h>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/windows.hpp>

namespace {

struct relationship_header {
    LOGICAL_PROCESSOR_RELATIONSHIP relationship;
    DWORD size;
};

} // namespace

int main() {
    relationship_header records[2] = {
        {RelationProcessorCore, static_cast<DWORD>(sizeof(relationship_header))},
        {RelationProcessorCore, static_cast<DWORD>(sizeof(relationship_header))}};
    const auto parsed = syscape::detail::cpu_backend::parse_relationship_count(
        reinterpret_cast<const unsigned char*>(records),
        static_cast<DWORD>(sizeof(records)),
        RelationProcessorCore);
    if (!parsed || *parsed != 2U) { return 1; }

    relationship_header malformed = {RelationProcessorCore, 0U};
    if (syscape::detail::cpu_backend::parse_relationship_count(
            reinterpret_cast<const unsigned char*>(&malformed),
            static_cast<DWORD>(sizeof(malformed)), RelationProcessorCore)) {
        return 2;
    }

    const auto logical = syscape::cpu::online_logical_processor_count();
    const auto physical = syscape::cpu::online_physical_core_count();
    const auto packages = syscape::cpu::online_processor_package_count();
    if (!logical || !physical || !packages || *logical == 0U ||
        *packages > *physical || *physical > *logical) {
        return 3;
    }
    if (syscape::cpu::vendor_identifiers().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::model_names().error() !=
            std::errc::operation_not_supported) {
        return 4;
    }
    return 0;
}
