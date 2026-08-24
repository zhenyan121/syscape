#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_LINUX_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_LINUX_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

#if defined(__i386__) || defined(__x86_64__)
#include <cpuid.h>
#endif

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

struct hypervisor_info {
    bool present = false;
    virtualization_common::hypervisor_type vendor =
        virtualization_common::hypervisor_type::none;
    std::string name;
};

struct container_info {
    bool present = false;
    virtualization_common::container_type runtime =
        virtualization_common::container_type::none;
    std::string name;
};

struct wsl_info {
    bool is_wsl = false;
    std::uint32_t version = 0U;
};

struct sandbox_info {
    bool is_sandboxed = false;
    virtualization_common::sandbox_type type =
        virtualization_common::sandbox_type::none;
};

/// Probes x86 / x86_64 CPUID leaves for hypervisor presence and signature.
inline hypervisor_info probe_cpuid_hypervisor() noexcept {
    hypervisor_info info;
#if defined(__i386__) || defined(__x86_64__)
    unsigned int eax = 0U;
    unsigned int ebx = 0U;
    unsigned int ecx = 0U;
    unsigned int edx = 0U;

    if (__get_cpuid(1U, &eax, &ebx, &ecx, &edx) != 0) {
        constexpr unsigned int hypervisor_bit = 1U << 31U;
        if ((ecx & hypervisor_bit) != 0U) {
            info.present = true;
            if (__get_cpuid(0x40000000U, &eax, &ebx, &ecx, &edx) != 0) {
                info.name = virtualization_common::decode_cpuid_signature(
                    ebx, ecx, edx);
                info.vendor =
                    virtualization_common::classify_cpuid_signature(info.name);
            } else {
                info.vendor = virtualization_common::hypervisor_type::unknown;
            }
        }
    }
#endif
    return info;
}

/// Reads one sysfs DMI attribute, returning empty string on absence.
inline std::string read_dmi_attribute(const char* name) {
    const std::string path = std::string("/sys/class/dmi/id/") + name;
    const result<std::string> content =
        linux_platform::read_text_file(path.c_str(), 1024U);
    if (!content) { return std::string(); }
    std::string value = *content;
    linux_platform::trim_line_end(value);
    return value;
}

/// Classifies the Linux sysfs hypervisor type token.
inline virtualization_common::hypervisor_type classify_sysfs_hypervisor_type(
    std::string_view type_name) noexcept {
    if (type_name.empty()) {
        return virtualization_common::hypervisor_type::unknown;
    }
    if (virtualization_common::case_insensitive_equal(type_name, "xen")) {
        return virtualization_common::hypervisor_type::xen;
    }
    return virtualization_common::hypervisor_type::other;
}

/// Detects hypervisor from CPUID, DMI attributes, and kernel sysfs.
inline result<hypervisor_info> detect_hypervisor() {
    hypervisor_info info = probe_cpuid_hypervisor();
    if (info.present &&
        info.vendor != virtualization_common::hypervisor_type::unknown &&
        info.vendor != virtualization_common::hypervisor_type::none &&
        info.vendor != virtualization_common::hypervisor_type::other) {
        return info;
    }

    // Check /sys/hypervisor/type
    const result<std::string> hypervisor_type_file =
        linux_platform::read_text_file("/sys/hypervisor/type", 256U);
    if (hypervisor_type_file) {
        std::string type_name = *hypervisor_type_file;
        linux_platform::trim_line_end(type_name);
        if (!type_name.empty()) {
            info.present = true;
            if (info.name.empty()) { info.name = type_name; }
            info.vendor = classify_sysfs_hypervisor_type(type_name);
            return info;
        }
    }

    // Check DMI vendor / product / bios strings
    const std::string sys_vendor = read_dmi_attribute("sys_vendor");
    const std::string product_name = read_dmi_attribute("product_name");
    const std::string bios_vendor = read_dmi_attribute("bios_vendor");

    const virtualization_common::hypervisor_type dmi_vendor =
        virtualization_common::classify_dmi_strings(sys_vendor, product_name,
                                                    bios_vendor);
    if (dmi_vendor != virtualization_common::hypervisor_type::unknown) {
        info.present = true;
        info.vendor = dmi_vendor;
        if (info.name.empty()) {
            info.name = !product_name.empty() ? product_name : sys_vendor;
        }
        return info;
    }

    // Check Device Tree for ARM/RISC-V virtualization
    if (::access("/proc/device-tree/hypervisor", F_OK) == 0) {
        info.present = true;
        if (info.vendor == virtualization_common::hypervisor_type::none) {
            info.vendor = virtualization_common::hypervisor_type::unknown;
        }
        if (info.name.empty()) { info.name = "DeviceTree Hypervisor"; }
        return info;
    }

    return info;
}

/// Detects WSL execution and major version (1 or 2).
inline result<wsl_info> detect_wsl() {
    wsl_info info;
    bool wsl_hint = false;

    if (::access("/proc/sys/fs/binfmt_misc/WSLInterop", F_OK) == 0 ||
        ::access("/run/WSL", F_OK) == 0) {
        wsl_hint = true;
    }

    const result<std::string> version_content =
        linux_platform::read_text_file("/proc/version", 4096U);
    if (version_content) {
        if (virtualization_common::case_insensitive_contains(*version_content,
                                                             "microsoft") ||
            virtualization_common::case_insensitive_contains(*version_content,
                                                             "wsl")) {
            wsl_hint = true;
        }
    }

    if (!wsl_hint) {
        return info;
    }

    info.is_wsl = true;
    info.version = 1U;

    // Check for WSL 2 indicators: "microsoft-standard-WSL2" or /dev/dxg
    if (version_content) {
        if (virtualization_common::case_insensitive_contains(
                *version_content, "wsl2") ||
            virtualization_common::case_insensitive_contains(
                *version_content, "microsoft-standard-wsl2")) {
            info.version = 2U;
            return info;
        }
    }

    const result<std::string> osrelease_content =
        linux_platform::read_text_file("/proc/sys/kernel/osrelease", 1024U);
    if (osrelease_content) {
        if (virtualization_common::case_insensitive_contains(
                *osrelease_content, "wsl2") ||
            virtualization_common::case_insensitive_contains(
                *osrelease_content, "microsoft-standard-wsl2")) {
            info.version = 2U;
            return info;
        }
    }

    if (::access("/dev/dxg", F_OK) == 0) {
        info.version = 2U;
        return info;
    }

    return info;
}

/// Detects container runtime from systemd container file, cgroups, and known paths.
inline result<container_info> detect_container() {
    container_info info;

    // 1. Check /run/systemd/container
    const result<std::string> systemd_container =
        linux_platform::read_text_file("/run/systemd/container", 256U);
    if (systemd_container) {
        std::string name = *systemd_container;
        linux_platform::trim_line_end(name);
        if (!name.empty()) {
            info.present = true;
            info.name = name;
            info.runtime =
                virtualization_common::classify_container_name(name);
            return info;
        }
    }

    // 2. Check /.dockerenv
    if (::access("/.dockerenv", F_OK) == 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::docker;
        info.name = "docker";
        return info;
    }

    // 3. Check /.containerenv
    if (::access("/.containerenv", F_OK) == 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::podman;
        info.name = "podman";
        return info;
    }

    // 4. Check /proc/vz for OpenVZ
    if (::access("/proc/vz", F_OK) == 0 && ::access("/proc/bc", F_OK) != 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::openvz;
        info.name = "openvz";
        return info;
    }

    // 5. Inspect /proc/1/cgroup and /proc/self/cgroup
    const auto check_cgroup_file = [](const char* path,
                                      container_info& target) -> bool {
        const result<std::string> cgroup_content =
            linux_platform::read_text_file(path, 16U * 1024U);
        if (!cgroup_content) { return false; }
        const std::string_view content_view = *cgroup_content;
        std::size_t offset = 0U;
        while (offset < content_view.size()) {
            const std::size_t end = content_view.find('\n', offset);
            const std::string_view line = content_view.substr(
                offset, end == std::string_view::npos
                            ? content_view.size() - offset
                            : end - offset);
            offset = end == std::string_view::npos ? content_view.size()
                                                   : end + 1U;

            const virtualization_common::container_type classified =
                virtualization_common::classify_cgroup_line(line);
            if (classified != virtualization_common::container_type::none) {
                target.present = true;
                target.runtime = classified;
                switch (classified) {
                case virtualization_common::container_type::docker:
                    target.name = "docker";
                    break;
                case virtualization_common::container_type::podman:
                    target.name = "podman";
                    break;
                case virtualization_common::container_type::kubernetes:
                    target.name = "kubernetes";
                    break;
                case virtualization_common::container_type::lxc:
                    target.name = "lxc";
                    break;
                case virtualization_common::container_type::lxd:
                    target.name = "lxd";
                    break;
                case virtualization_common::container_type::containerd:
                    target.name = "containerd";
                    break;
                default:
                    target.name = "container";
                    break;
                }
                return true;
            }
        }
        return false;
    };

    if (check_cgroup_file("/proc/1/cgroup", info) ||
        check_cgroup_file("/proc/self/cgroup", info)) {
        return info;
    }

    // 6. Check WSL
    const result<wsl_info> wsl = detect_wsl();
    if (wsl && wsl->is_wsl) {
        info.present = true;
        info.runtime = virtualization_common::container_type::wsl;
        info.name = "wsl";
        return info;
    }

    return info;
}

/// Detects application sandbox mechanisms (Flatpak, Snap).
inline result<sandbox_info> detect_sandbox() {
    sandbox_info info;

    if (::access("/.flatpak-info", F_OK) == 0) {
        info.is_sandboxed = true;
        info.type = virtualization_common::sandbox_type::flatpak;
        return info;
    }

    if (::getenv("SNAP") != nullptr || ::getenv("SNAP_NAME") != nullptr ||
        ::getenv("SNAP_INSTANCE_NAME") != nullptr) {
        info.is_sandboxed = true;
        info.type = virtualization_common::sandbox_type::snap;
        return info;
    }

    return info;
}

inline result<bool> is_hypervisor_present() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    return info->present;
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    return info->vendor;
}

inline result<std::string> hypervisor_name() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) { return fail(info.error()); }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_container() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    return info->present;
}

inline result<virtualization_common::container_type> container() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    return info->runtime;
}

inline result<std::string> container_name() {
    const result<container_info> info = detect_container();
    if (!info) { return fail(info.error()); }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_wsl() {
    const result<wsl_info> info = detect_wsl();
    if (!info) { return fail(info.error()); }
    return info->is_wsl;
}

inline result<std::uint32_t> wsl_version() {
    const result<wsl_info> info = detect_wsl();
    if (!info) { return fail(info.error()); }
    if (!info->is_wsl) { return fail(errc::not_found); }
    return info->version;
}

inline result<bool> is_sandboxed() {
    const result<sandbox_info> info = detect_sandbox();
    if (!info) { return fail(info.error()); }
    return info->is_sandboxed;
}

inline result<virtualization_common::sandbox_type> sandbox() {
    const result<sandbox_info> info = detect_sandbox();
    if (!info) { return fail(info.error()); }
    return info->type;
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
