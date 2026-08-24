#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#include <syscape/virtualization.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/detail/virtualization/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpuid_signature_decoding() {
    namespace common = syscape::detail::virtualization_common;

    // "KVMKVMKVM\0\0\0"
    // ebx = 'K' | ('V'<<8) | ('M'<<16) | ('K'<<24) = 0x4B4D564B
    // ecx = 'V' | ('M'<<8) | ('K'<<16) | ('V'<<24) = 0x564B4D56
    // edx = 'M' | (0<<8)   | (0<<16)   | (0<<24)   = 0x0000004D
    const std::string kvm = common::decode_cpuid_signature(
        0x4B4D564BU, 0x564B4D56U, 0x0000004DU);
    expect(kvm == "KVMKVMKVM", "KVM signature must decode and trim trailing nulls");
    expect(common::classify_cpuid_signature(kvm) ==
               common::hypervisor_type::kvm,
           "KVM signature must classify as KVM");

    // VMware: "VMwareVMware"
    // ebx: "VMwa" = 0x61774D56
    // ecx: "reVM" = 0x4D566572
    // edx: "ware" = 0x65726177
    const std::string vmware = common::decode_cpuid_signature(
        0x61774D56U, 0x4D566572U, 0x65726177U);
    expect(vmware == "VMwareVMware", "VMware signature must decode exactly");
    expect(common::classify_cpuid_signature(vmware) ==
               common::hypervisor_type::vmware,
           "VMware signature must classify as VMware");

    // Hyper-V: "Microsoft Hv"
    expect(common::classify_cpuid_signature("Microsoft Hv") ==
               common::hypervisor_type::hyper_v,
           "Microsoft Hv must classify as Hyper-V");

    // VirtualBox: "VBoxVBoxVBox"
    expect(common::classify_cpuid_signature("VBoxVBoxVBox") ==
               common::hypervisor_type::virtualbox,
           "VBoxVBoxVBox must classify as VirtualBox");

    // QEMU TCG: "TCGTCGTCGTCG"
    expect(common::classify_cpuid_signature("TCGTCGTCGTCG") ==
               common::hypervisor_type::qemu,
           "TCGTCGTCGTCG must classify as QEMU");

    // Xen: "XenVMMXenVMM"
    expect(common::classify_cpuid_signature("XenVMMXenVMM") ==
               common::hypervisor_type::xen,
           "XenVMMXenVMM must classify as Xen");

    // bhyve: "bhyve bhyve "
    expect(common::classify_cpuid_signature("bhyve bhyve ") ==
               common::hypervisor_type::bhyve,
           "bhyve signature must classify as bhyve");

    // Parallels: "prl hyperv  "
    expect(common::classify_cpuid_signature("prl hyperv  ") ==
               common::hypervisor_type::parallels,
           "prl hyperv must classify as Parallels");

    // ACRN: "ACRNACRNACRN"
    expect(common::classify_cpuid_signature("ACRNACRNACRN") ==
               common::hypervisor_type::acrn,
           "ACRN signature must classify as ACRN");

    // QNX: "QNXQVMBSYS\0"
    expect(common::classify_cpuid_signature("QNXQVMBSYS\0") ==
               common::hypervisor_type::qnx_hypervisor,
           "QNX signature must classify as QNX");

    // Other unrecognized signature
    expect(common::classify_cpuid_signature("CustomHV123") ==
               common::hypervisor_type::other,
           "Unrecognized non-empty signature must classify as other");

    // Empty signature
    expect(common::classify_cpuid_signature("") ==
               common::hypervisor_type::unknown,
           "Empty signature must classify as unknown");
}

void test_ascii_case_folding() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::case_insensitive_contains("MICROsoft Hv", "microsoft"),
           "ASCII containment must ignore letter case");
    expect(common::case_insensitive_equal("XeN", "xEn"),
           "ASCII equality must ignore letter case");
    expect(!common::case_insensitive_equal("Xen", "Xeo"),
           "ASCII equality must still distinguish different letters");
}

void test_sysfs_hypervisor_type_classification() {
    namespace backend = syscape::detail::virtualization_backend;
    namespace common = syscape::detail::virtualization_common;

    expect(backend::classify_sysfs_hypervisor_type("XEN") ==
               common::hypervisor_type::xen,
           "The Xen sysfs type must classify as Xen case-insensitively");
    expect(backend::classify_sysfs_hypervisor_type("custom-hypervisor") ==
               common::hypervisor_type::other,
           "A non-Xen sysfs type must classify as another present hypervisor");
    expect(backend::classify_sysfs_hypervisor_type("") ==
               common::hypervisor_type::unknown,
           "An empty sysfs type must not fabricate a vendor");
}

void test_dmi_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::classify_dmi_strings("QEMU", "Standard PC", "Bochs") ==
               common::hypervisor_type::qemu,
           "QEMU DMI strings must classify as QEMU");
    expect(common::classify_dmi_strings("innotek GmbH", "VirtualBox", "innotek") ==
               common::hypervisor_type::virtualbox,
           "VirtualBox DMI strings must classify as VirtualBox");
    expect(common::classify_dmi_strings("VMware, Inc.", "VMware Virtual Platform", "Phoenix") ==
               common::hypervisor_type::vmware,
           "VMware DMI strings must classify as VMware");
    expect(common::classify_dmi_strings("Microsoft Corporation", "Virtual Machine", "American Megatrends") ==
               common::hypervisor_type::hyper_v,
           "Microsoft VM DMI strings must classify as Hyper-V");
    expect(common::classify_dmi_strings("Xen", "HVM domU", "Xen") ==
               common::hypervisor_type::xen,
           "Xen DMI strings must classify as Xen");
    expect(common::classify_dmi_strings("Amazon EC2", "t3.medium", "Amazon EC2") ==
               common::hypervisor_type::kvm,
           "Amazon EC2 must classify as KVM-based cloud hypervisor");
    expect(common::classify_dmi_strings("Google", "Google Compute Engine", "Google") ==
               common::hypervisor_type::kvm,
           "Google Compute Engine must classify as KVM");
    expect(common::classify_dmi_strings("LENOVO", "20XX", "LENOVO") ==
               common::hypervisor_type::unknown,
           "Physical bare-metal DMI strings must return unknown");
}

void test_container_classification() {
    namespace common = syscape::detail::virtualization_common;

    expect(common::classify_container_name("docker") ==
               common::container_type::docker,
           "docker must classify as docker");
    expect(common::classify_container_name("podman") ==
               common::container_type::podman,
           "podman must classify as podman");
    expect(common::classify_container_name("lxc") ==
               common::container_type::lxc,
           "lxc must classify as lxc");
    expect(common::classify_container_name("lxd") ==
               common::container_type::lxd,
           "lxd must classify as lxd");
    expect(common::classify_container_name("containerd") ==
               common::container_type::containerd,
           "containerd must classify as containerd");
    expect(common::classify_container_name("systemd-nspawn") ==
               common::container_type::systemd_nspawn,
           "systemd-nspawn must classify as systemd_nspawn");
    expect(common::classify_container_name("wsl") ==
               common::container_type::wsl,
           "wsl must classify as wsl");
    expect(common::classify_container_name("custom-rt") ==
               common::container_type::other,
           "Custom container name must classify as other");
    expect(common::classify_container_name("") ==
               common::container_type::unknown,
           "Empty container name must classify as unknown");

    // cgroup line matching
    expect(common::classify_cgroup_line("12:memory:/docker/1234567890abcdef") ==
               common::container_type::docker,
           "cgroup docker line must classify as docker");
    expect(common::classify_cgroup_line("0::/system.slice/docker-1234.scope") ==
               common::container_type::docker,
           "cgroup v2 docker line must classify as docker");
    expect(common::classify_cgroup_line("0::/user.slice/user-1000.slice/libpod-123.scope") ==
               common::container_type::podman,
           "cgroup podman line must classify as podman");
    expect(common::classify_cgroup_line("1:name=systemd:/kubepods/burstable/pod123/456") ==
               common::container_type::kubernetes,
           "cgroup kubepods line must classify as kubernetes");
    expect(common::classify_cgroup_line("0::/lxc/my-container") ==
               common::container_type::lxc,
           "cgroup lxc line must classify as lxc");
    expect(common::classify_cgroup_line("0::/user.slice/user-1000.slice/session-1.scope") ==
               common::container_type::none,
           "Normal user cgroup line must classify as none");
}

void test_live_queries() {
    const auto hv_present = syscape::virtualization::is_hypervisor_present();
    expect(hv_present.has_value(), "is_hypervisor_present must succeed");
    if (hv_present) {
        const auto hv_vendor = syscape::virtualization::hypervisor();
        expect(hv_vendor.has_value(), "hypervisor query must succeed");
        if (*hv_present) {
            expect(*hv_vendor != syscape::virtualization::hypervisor_vendor::none,
                   "If hypervisor is present, vendor must not be none");
            const auto name = syscape::virtualization::hypervisor_name();
            expect(name.has_value() && !name->empty(),
                   "If hypervisor is present, hypervisor_name should return non-empty name");
        } else {
            expect(*hv_vendor == syscape::virtualization::hypervisor_vendor::none,
                   "If hypervisor is not present, vendor must be none");
            const auto name = syscape::virtualization::hypervisor_name();
            expect(!name && name.error() == syscape::errc::not_found,
                   "If hypervisor is not present, hypervisor_name must return not_found");
        }
    }

    const auto cont_present = syscape::virtualization::is_container();
    expect(cont_present.has_value(), "is_container must succeed");
    if (cont_present) {
        const auto cont_rt = syscape::virtualization::container();
        expect(cont_rt.has_value(), "container query must succeed");
        if (*cont_present) {
            expect(*cont_rt != syscape::virtualization::container_runtime::none,
                   "If container is present, runtime must not be none");
            const auto name = syscape::virtualization::container_name();
            expect(name.has_value() && !name->empty(),
                   "If container is present, container_name should return name");
        } else {
            expect(*cont_rt == syscape::virtualization::container_runtime::none,
                   "If container is not present, runtime must be none");
            const auto name = syscape::virtualization::container_name();
            expect(!name && name.error() == syscape::errc::not_found,
                   "If not in container, container_name must return not_found");
        }
    }

    const auto wsl_check = syscape::virtualization::is_wsl();
    expect(wsl_check.has_value(), "is_wsl must succeed");
    if (wsl_check) {
        const auto ver = syscape::virtualization::wsl_version();
        if (*wsl_check) {
            expect(ver.has_value() && (*ver == 1U || *ver == 2U),
                   "If running in WSL, wsl_version must be 1 or 2");
        } else {
            expect(!ver && ver.error() == syscape::errc::not_found,
                   "If not in WSL, wsl_version must report not_found");
        }
    }

    const auto sandboxed = syscape::virtualization::is_sandboxed();
    expect(sandboxed.has_value(), "is_sandboxed must succeed");
    if (sandboxed) {
        const auto sb_type = syscape::virtualization::sandbox();
        expect(sb_type.has_value(), "sandbox query must succeed");
        if (*sandboxed) {
            expect(*sb_type != syscape::virtualization::sandbox_type::none,
                   "If sandboxed, sandbox_type must not be none");
        } else {
            expect(*sb_type == syscape::virtualization::sandbox_type::none,
                   "If not sandboxed, sandbox_type must be none");
        }
    }
}

} // namespace

int main() {
    test_cpuid_signature_decoding();
    test_ascii_case_folding();
    test_sysfs_hypervisor_type_classification();
    test_dmi_classification();
    test_container_classification();
    test_live_queries();
    return failures == 0 ? 0 : 1;
}
