#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <windows.h>

#include <syscape/virtualization.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_cpuid_parsing() {
    namespace backend = syscape::detail::virtualization_backend;
    namespace common = syscape::detail::virtualization_common;

    // Hypervisor bit set (ecx bit 31), Hyper-V signature "Microsoft Hv"
    // "Micr" = 0x7263694D
    // "osof" = 0x666F736F
    // "t Hv" = 0x76482074
    const backend::hypervisor_info hyperv =
        backend::parse_cpuid_hypervisor(
            1U << 31U, 0x7263694DU, 0x666F736FU, 0x76482074U);
    expect(hyperv.present, "Hypervisor bit must be recognized as present");
    expect(hyperv.vendor == common::hypervisor_type::hyper_v,
           "Microsoft Hv must classify as Hyper-V");
    expect(hyperv.name == "Microsoft Hv",
           "Decoded signature must match Microsoft Hv");

    // Hypervisor bit clear (bare metal)
    const backend::hypervisor_info bare_metal =
        backend::parse_cpuid_hypervisor(
            0U, 0x7263694DU, 0x666F736FU, 0x76482074U);
    expect(!bare_metal.present, "Absence of hypervisor bit must indicate bare metal");
    expect(bare_metal.vendor == common::hypervisor_type::none,
           "Bare metal must report none vendor");
}

} // namespace

int main() {
    test_windows_cpuid_parsing();
    return failures == 0 ? 0 : 1;
}
