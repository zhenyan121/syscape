#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/virtualization.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_macos_interpretation() {
    namespace backend = syscape::detail::virtualization_backend;
    namespace common = syscape::detail::virtualization_common;

    // hv_vmm_present = 1 -> Apple Hypervisor
    const backend::hypervisor_info hv1 =
        backend::interpret_macos_hypervisor(1, "", "");
    expect(hv1.present, "hv_vmm_present=1 must indicate hypervisor presence");
    expect(hv1.vendor == common::hypervisor_type::apple_hypervisor,
           "hv_vmm_present=1 must classify as Apple Hypervisor");

    // VMM in cpu_features -> Apple Hypervisor
    const backend::hypervisor_info hv2 =
        backend::interpret_macos_hypervisor(0, "FPU VME DE PSE TSC MSR PAE VMM", "");
    expect(hv2.present, "VMM in features must indicate hypervisor presence");
    expect(hv2.vendor == common::hypervisor_type::apple_hypervisor,
           "VMM in features must classify as Apple Hypervisor");

    // VMware model string -> VMware
    const backend::hypervisor_info hv3 =
        backend::interpret_macos_hypervisor(0, "", "VMware7,1");
    expect(hv3.present, "VMware model must indicate hypervisor presence");
    expect(hv3.vendor == common::hypervisor_type::vmware,
           "VMware model must classify as VMware");

    // Parallels model string -> Parallels
    const backend::hypervisor_info hv4 =
        backend::interpret_macos_hypervisor(0, "", "Parallels Virtual Platform");
    expect(hv4.present, "Parallels model must indicate hypervisor presence");
    expect(hv4.vendor == common::hypervisor_type::parallels,
           "Parallels model must classify as Parallels");

    // Bare metal Mac
    const backend::hypervisor_info bare_metal =
        backend::interpret_macos_hypervisor(0, "FPU VME DE PSE TSC MSR", "MacBookPro18,1");
    expect(!bare_metal.present, "Bare metal Mac must not report hypervisor presence");
    expect(bare_metal.vendor == common::hypervisor_type::none,
           "Bare metal Mac must report none vendor");
}

} // namespace

int main() {
    test_macos_interpretation();
    return failures == 0 ? 0 : 1;
}
