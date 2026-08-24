#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_COMMON_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_COMMON_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_common {

using hypervisor_type = ::syscape::virtualization::hypervisor_vendor;
using container_type = ::syscape::virtualization::container_runtime;
using sandbox_type = ::syscape::virtualization::sandbox_type;

/// Converts one ASCII uppercase letter to lowercase without consulting a locale.
inline unsigned char ascii_lower(unsigned char value) noexcept {
    return value >= static_cast<unsigned char>('A') &&
                   value <= static_cast<unsigned char>('Z')
               ? static_cast<unsigned char>(
                     value + static_cast<unsigned char>('a' - 'A'))
               : value;
}

/// Validates one converted identity text string at the public boundary.
inline result<std::string> validate_identity_text(result<std::string> value) {
    if (!value) { return fail(value.error()); }
    if (value->empty()) { return fail(errc::malformed_data); }
    if (!is_valid_utf8(*value)) { return fail(errc::invalid_encoding); }
    return value;
}

/// Checks if string `text` contains `needle` case-insensitively.
inline bool case_insensitive_contains(std::string_view text,
                                      std::string_view needle) noexcept {
    if (needle.empty()) { return true; }
    if (needle.size() > text.size()) { return false; }
    for (std::size_t i = 0; i <= text.size() - needle.size(); ++i) {
        bool match = true;
        for (std::size_t j = 0; j < needle.size(); ++j) {
            const auto c1 = static_cast<unsigned char>(text[i + j]);
            const auto c2 = static_cast<unsigned char>(needle[j]);
            if (ascii_lower(c1) != ascii_lower(c2)) {
                match = false;
                break;
            }
        }
        if (match) { return true; }
    }
    return false;
}

/// Checks if two strings are equal case-insensitively.
inline bool case_insensitive_equal(std::string_view a,
                                    std::string_view b) noexcept {
    if (a.size() != b.size()) { return false; }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const auto c1 = static_cast<unsigned char>(a[i]);
        const auto c2 = static_cast<unsigned char>(b[i]);
        if (ascii_lower(c1) != ascii_lower(c2)) { return false; }
    }
    return true;
}

/// Trims trailing null bytes and whitespace from a signature string.
inline std::string_view trim_signature(std::string_view input) noexcept {
    while (!input.empty() &&
           (input.back() == '\0' || input.back() == ' ' || input.back() == '\t' ||
            input.back() == '\r' || input.back() == '\n')) {
        input.remove_suffix(1U);
    }
    while (!input.empty() &&
           (input.front() == '\0' || input.front() == ' ' || input.front() == '\t' ||
            input.front() == '\r' || input.front() == '\n')) {
        input.remove_prefix(1U);
    }
    return input;
}

/// Decodes the 12-byte CPUID leaf 0x40000000 hypervisor signature.
inline std::string decode_cpuid_signature(std::uint32_t ebx,
                                          std::uint32_t ecx,
                                          std::uint32_t edx) {
    char buffer[12];
    buffer[0] = static_cast<char>(ebx & 0xFFU);
    buffer[1] = static_cast<char>((ebx >> 8U) & 0xFFU);
    buffer[2] = static_cast<char>((ebx >> 16U) & 0xFFU);
    buffer[3] = static_cast<char>((ebx >> 24U) & 0xFFU);
    buffer[4] = static_cast<char>(ecx & 0xFFU);
    buffer[5] = static_cast<char>((ecx >> 8U) & 0xFFU);
    buffer[6] = static_cast<char>((ecx >> 16U) & 0xFFU);
    buffer[7] = static_cast<char>((ecx >> 24U) & 0xFFU);
    buffer[8] = static_cast<char>(edx & 0xFFU);
    buffer[9] = static_cast<char>((edx >> 8U) & 0xFFU);
    buffer[10] = static_cast<char>((edx >> 16U) & 0xFFU);
    buffer[11] = static_cast<char>((edx >> 24U) & 0xFFU);
    const std::string_view trimmed =
        trim_signature(std::string_view(buffer, 12U));
    return std::string(trimmed);
}

/// Maps one CPUID hypervisor signature string onto the hypervisor_vendor vocabulary.
inline hypervisor_type classify_cpuid_signature(std::string_view sig) noexcept {
    const std::string_view trimmed = trim_signature(sig);
    if (trimmed == "KVMKVMKVM") { return hypervisor_type::kvm; }
    if (trimmed == "TCGTCGTCGTCG") { return hypervisor_type::qemu; }
    if (trimmed == "VMwareVMware") { return hypervisor_type::vmware; }
    if (trimmed == "Microsoft Hv") { return hypervisor_type::hyper_v; }
    if (trimmed == "XenVMMXenVMM") { return hypervisor_type::xen; }
    if (trimmed == "bhyve bhyve") { return hypervisor_type::bhyve; }
    if (trimmed == "prl hyperv") {
        return hypervisor_type::parallels;
    }
    if (trimmed == "VBoxVBoxVBox") { return hypervisor_type::virtualbox; }
    if (trimmed == "ACRNACRNACRN") { return hypervisor_type::acrn; }
    if (trimmed == "QNXQVMBSYS") { return hypervisor_type::qnx_hypervisor; }
    if (trimmed == "Apple VMM") { return hypervisor_type::apple_hypervisor; }
    if (!trimmed.empty()) { return hypervisor_type::other; }
    return hypervisor_type::unknown;
}

/// Classifies hypervisor vendor from firmware / DMI strings.
inline hypervisor_type classify_dmi_strings(std::string_view vendor,
                                            std::string_view product,
                                            std::string_view bios_vendor) noexcept {
    if (case_insensitive_contains(vendor, "QEMU") ||
        case_insensitive_contains(product, "QEMU") ||
        case_insensitive_contains(bios_vendor, "QEMU") ||
        case_insensitive_contains(product, "Bochs") ||
        case_insensitive_contains(bios_vendor, "Bochs")) {
        return hypervisor_type::qemu;
    }
    if (case_insensitive_contains(vendor, "VirtualBox") ||
        case_insensitive_contains(product, "VirtualBox") ||
        case_insensitive_contains(bios_vendor, "VirtualBox") ||
        case_insensitive_contains(vendor, "innotek")) {
        return hypervisor_type::virtualbox;
    }
    if (case_insensitive_contains(vendor, "VMware") ||
        case_insensitive_contains(product, "VMware") ||
        case_insensitive_contains(bios_vendor, "VMware")) {
        return hypervisor_type::vmware;
    }
    if (case_insensitive_contains(vendor, "KVM") ||
        case_insensitive_contains(product, "KVM") ||
        case_insensitive_contains(bios_vendor, "KVM")) {
        return hypervisor_type::kvm;
    }
    if (case_insensitive_contains(vendor, "Xen") ||
        case_insensitive_contains(product, "Xen") ||
        case_insensitive_contains(bios_vendor, "Xen")) {
        return hypervisor_type::xen;
    }
    if (case_insensitive_contains(vendor, "Microsoft") &&
        case_insensitive_contains(product, "Virtual Machine")) {
        return hypervisor_type::hyper_v;
    }
    if (case_insensitive_contains(vendor, "Parallels") ||
        case_insensitive_contains(product, "Parallels")) {
        return hypervisor_type::parallels;
    }
    if (case_insensitive_contains(vendor, "bhyve") ||
        case_insensitive_contains(product, "bhyve")) {
        return hypervisor_type::bhyve;
    }
    if (case_insensitive_contains(vendor, "Apple") &&
        (case_insensitive_contains(product, "Virtual") ||
         case_insensitive_contains(product, "Apple Virtualization"))) {
        return hypervisor_type::apple_hypervisor;
    }
    if (case_insensitive_contains(vendor, "Amazon EC2") ||
        case_insensitive_contains(vendor, "Google") ||
        case_insensitive_contains(product, "Google Compute Engine")) {
        // Major cloud hypervisors run KVM-based virtualization.
        return hypervisor_type::kvm;
    }
    return hypervisor_type::unknown;
}

/// Maps a container runtime token string onto the container_runtime enum.
inline container_type classify_container_name(std::string_view name) noexcept {
    const std::string_view trimmed = trim_signature(name);
    if (case_insensitive_equal(trimmed, "docker")) {
        return container_type::docker;
    }
    if (case_insensitive_equal(trimmed, "podman")) {
        return container_type::podman;
    }
    if (case_insensitive_equal(trimmed, "lxc") ||
        case_insensitive_equal(trimmed, "lxc-libvirt")) {
        return container_type::lxc;
    }
    if (case_insensitive_equal(trimmed, "lxd")) {
        return container_type::lxd;
    }
    if (case_insensitive_equal(trimmed, "containerd")) {
        return container_type::containerd;
    }
    if (case_insensitive_equal(trimmed, "kubernetes") ||
        case_insensitive_equal(trimmed, "k8s") ||
        case_insensitive_equal(trimmed, "kubepods")) {
        return container_type::kubernetes;
    }
    if (case_insensitive_equal(trimmed, "systemd-nspawn") ||
        case_insensitive_equal(trimmed, "nspawn")) {
        return container_type::systemd_nspawn;
    }
    if (case_insensitive_equal(trimmed, "openvz") ||
        case_insensitive_equal(trimmed, "vz")) {
        return container_type::openvz;
    }
    if (case_insensitive_equal(trimmed, "wsl")) {
        return container_type::wsl;
    }
    if (case_insensitive_equal(trimmed, "appbox")) {
        return container_type::appbox;
    }
    if (!trimmed.empty()) { return container_type::other; }
    return container_type::unknown;
}

/// Matches a cgroup path hierarchy against known container prefixes.
inline container_type classify_cgroup_line(std::string_view line) noexcept {
    if (case_insensitive_contains(line, "/docker/") ||
        case_insensitive_contains(line, "/docker-") ||
        case_insensitive_contains(line, "docker.service")) {
        return container_type::docker;
    }
    if (case_insensitive_contains(line, "/libpod-") ||
        case_insensitive_contains(line, "/podman/")) {
        return container_type::podman;
    }
    if (case_insensitive_contains(line, "/kubepods/") ||
        case_insensitive_contains(line, "/kubepods.slice/")) {
        return container_type::kubernetes;
    }
    if (case_insensitive_contains(line, "/lxc/") ||
        case_insensitive_contains(line, "/lxc.payload.")) {
        return container_type::lxc;
    }
    if (case_insensitive_contains(line, "/lxd/")) {
        return container_type::lxd;
    }
    if (case_insensitive_contains(line, "/containerd/") ||
        case_insensitive_contains(line, "containerd.service")) {
        return container_type::containerd;
    }
    return container_type::none;
}

} // namespace virtualization_common
} // namespace detail
} // namespace syscape

#endif
