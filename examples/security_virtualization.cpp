#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include <syscape/security.hpp>
#include <syscape/virtualization.hpp>

namespace {

const char* secure_boot_name(syscape::security::secure_boot_state state) {
    switch (state) {
    case syscape::security::secure_boot_state::enabled: return "Enabled (Enforcing)";
    case syscape::security::secure_boot_state::disabled: return "Disabled";
    case syscape::security::secure_boot_state::audit: return "Audit / Setup Mode";
    case syscape::security::secure_boot_state::unknown: return "Unknown / Not Applicable";
    }
    return "Unknown";
}

const char* tpm_version_name(syscape::security::tpm_version ver) {
    switch (ver) {
    case syscape::security::tpm_version::none: return "No TPM Detected";
    case syscape::security::tpm_version::v1_2: return "TPM 1.2";
    case syscape::security::tpm_version::v2_0: return "TPM 2.0";
    case syscape::security::tpm_version::other: return "Other TPM Version";
    case syscape::security::tpm_version::unknown: return "Unknown";
    }
    return "Unknown";
}

const char* aslr_mode_name(syscape::security::aslr_mode mode) {
    switch (mode) {
    case syscape::security::aslr_mode::full: return "Full (Stack, Heap, Mmap, VDSO)";
    case syscape::security::aslr_mode::partial: return "Partial (Stack, Mmap)";
    case syscape::security::aslr_mode::disabled: return "Disabled";
    case syscape::security::aslr_mode::unknown: return "Unknown";
    }
    return "Unknown";
}

const char* mitigation_name(syscape::security::mitigation_status status) {
    switch (status) {
    case syscape::security::mitigation_status::mitigated: return "Mitigated (Protected)";
    case syscape::security::mitigation_status::not_affected: return "Not Affected (Hardware Immune)";
    case syscape::security::mitigation_status::vulnerable: return "Vulnerable";
    case syscape::security::mitigation_status::disabled: return "Mitigation Disabled";
    case syscape::security::mitigation_status::unknown: return "Unknown";
    }
    return "Unknown";
}

const char* cgroup_name(syscape::virtualization::cgroup_version ver) {
    switch (ver) {
    case syscape::virtualization::cgroup_version::v1: return "cgroup v1 (Legacy)";
    case syscape::virtualization::cgroup_version::v2: return "cgroup v2 (Unified)";
    case syscape::virtualization::cgroup_version::hybrid: return "Hybrid (v1 + v2)";
    case syscape::virtualization::cgroup_version::none: return "None / Unmounted";
    }
    return "None";
}

} // namespace

int main() {
    std::cout << "=== Syscape Security & Virtualization Example ===" << std::endl;

    // Platform Security Posture
    std::cout << "\n[System Security Posture]" << std::endl;
    if (const auto sb = syscape::security::secure_boot()) {
        std::cout << "  Secure Boot:   " << secure_boot_name(*sb) << std::endl;
    }
    if (const auto tpm = syscape::security::tpm()) {
        std::cout << "  TPM Device:    " << tpm_version_name(tpm->version);
        if (!tpm->version_string.empty()) {
            std::cout << " (" << tpm->version_string << ")";
        }
        std::cout << std::endl;
    }
    if (const auto aslr = syscape::security::aslr()) {
        std::cout << "  ASLR Mode:     " << aslr_mode_name(*aslr) << std::endl;
    }

    // CPU Hardware Vulnerability Defenses
    std::cout << "\n[CPU Hardware Vulnerability Defenses]" << std::endl;
    if (const auto vulns = syscape::security::cpu_vulnerabilities()) {
        for (const auto& v : *vulns) {
            std::cout << "  " << std::left << std::setw(16) << v.name << ": "
                      << mitigation_name(v.status) << std::endl;
            if (!v.raw_description.empty()) {
                std::cout << "    [" << v.raw_description << "]" << std::endl;
            }
        }
    }

    // Process Capabilities & Privileges
    std::cout << "\n[Process Security Capabilities]" << std::endl;
    if (const auto caps = syscape::security::capabilities()) {
        std::cout << "  Effective Capabilities (" << caps->effective.size() << "): ";
        if (caps->effective.empty()) {
            std::cout << "None (Standard Unprivileged Process)";
        } else {
            for (std::size_t i = 0; i < caps->effective.size(); ++i) {
                if (i > 0) { std::cout << ", "; }
                std::cout << caps->effective[i];
            }
        }
        std::cout << std::endl;
    }

    // Virtualization & Containers
    std::cout << "\n[Virtualization & Container Environment]" << std::endl;
    if (const auto is_hyp = syscape::virtualization::is_hypervisor_present()) {
        std::cout << "  Hypervisor:    " << (*is_hyp ? "Detected (Virtual Machine)" : "None (Bare Metal)") << std::endl;
        if (*is_hyp) {
            if (const auto hyp_name = syscape::virtualization::hypervisor_name()) {
                std::cout << "  Vendor Sig:    " << *hyp_name << std::endl;
            }
        }
    }
    if (const auto is_cont = syscape::virtualization::is_container()) {
        std::cout << "  In Container:  " << (*is_cont ? "Yes" : "No") << std::endl;
        if (*is_cont) {
            if (const auto cont_name = syscape::virtualization::container_name()) {
                std::cout << "  Container Run: " << *cont_name << std::endl;
            }
        }
    }
    if (const auto is_sand = syscape::virtualization::is_sandboxed()) {
        std::cout << "  Is Sandboxed:  " << (*is_sand ? "Yes" : "No") << std::endl;
    }
    if (const auto cg = syscape::virtualization::cgroup_hierarchy_version()) {
        std::cout << "  Control Group: " << cgroup_name(*cg) << std::endl;
    }

    // Namespace Isolation
    std::cout << "\n[Linux Namespaces]" << std::endl;
    if (const auto nss = syscape::virtualization::namespaces()) {
        std::cout << "  Process Namespaces (" << nss->size() << "):" << std::endl;
        for (const auto& ns : *nss) {
            std::cout << "    [" << ns.name << "] inode: " << ns.inode;
            if (ns.is_isolated) {
                std::cout << " (" << (*ns.is_isolated ? "Isolated / Private" : "Host Root NS") << ")";
            }
            std::cout << std::endl;
        }
    }

    return 0;
}
