#ifndef SYSCAPE_VIRTUALIZATION_HPP
#define SYSCAPE_VIRTUALIZATION_HPP

/// @file
/// @brief Hosted virtualization, hypervisor, container, WSL, and sandbox queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.
/// @note This module exposes:
/// - Hypervisor presence and classified vendor identity (e.g. KVM, QEMU, VMware,
///   Hyper-V, VirtualBox, Xen, bhyve, Parallels, Apple Hypervisor, ACRN, QNX).
/// - Container runtime detection (e.g. Docker, Podman, LXC, LXD, containerd,
///   Kubernetes, systemd-nspawn, OpenVZ, WSL, Appbox).
/// - Windows Subsystem for Linux (WSL) presence and version (WSL 1 vs WSL 2).
/// - Application sandbox detection (e.g. Flatpak, Snap, Apple App Sandbox,
///   Windows AppContainer).
/// @note Linux implements hypervisor queries through CPUID instruction leaves
/// (leaf 1 ECX hypervisor bit and leaf 0x40000000 signature), DMI sysfs attributes
/// under /sys/class/dmi/id, and /sys/hypervisor/type. Container queries inspect
/// /run/systemd/container, /.dockerenv, /.containerenv, cgroup path hierarchies,
/// and /proc/vz. WSL queries inspect WSL interop endpoints and kernel release
/// strings. Sandbox queries inspect Flatpak and Snap environment indicators.
/// @note Windows implements hypervisor queries through CPUID instruction leaves
/// and raw SMBIOS table inspection. Sandbox queries inspect process token
/// AppContainer classifications.
/// @note macOS implements hypervisor queries through sysctl kern.hv_vmm_present,
/// machdep.cpu.features VMM flags, and IOKit platform expert device matching.
/// Sandbox queries inspect the Apple sandbox environment.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/virtualization.hpp requires C++17 or later"
#endif

#include <cstdint>
#include <string>

namespace syscape {
namespace virtualization {

/// Classified hypervisor or virtual machine manager.
enum class hypervisor_vendor : std::uint8_t {
    /// A hypervisor is detected, but its vendor signature is unrecognized.
    unknown,
    /// Verified bare-metal execution with no detected hypervisor.
    none,
    /// Linux Kernel-based Virtual Machine.
    kvm,
    /// QEMU emulator or TCG hypervisor.
    qemu,
    /// VMware Workstation, ESXi, Player, or Fusion.
    vmware,
    /// Oracle VM VirtualBox.
    virtualbox,
    /// Microsoft Hyper-V.
    hyper_v,
    /// Xen Project Hypervisor.
    xen,
    /// FreeBSD / illumos bhyve hypervisor.
    bhyve,
    /// Parallels Desktop or Server.
    parallels,
    /// Apple Hypervisor / Virtualization framework.
    apple_hypervisor,
    /// ACRN embedded hypervisor.
    acrn,
    /// QNX Hypervisor.
    qnx_hypervisor,
    /// Other classified hypervisor.
    other
};

/// Classified container runtime environment.
enum class container_runtime : std::uint8_t {
    /// Container execution is detected, but the runtime is unrecognized.
    unknown,
    /// Execution is not within a detected container.
    none,
    /// Docker container engine.
    docker,
    /// Podman container engine.
    podman,
    /// Linux Containers (LXC).
    lxc,
    /// LXD system container manager.
    lxd,
    /// containerd runtime.
    containerd,
    /// Kubernetes pod.
    kubernetes,
    /// systemd-nspawn container.
    systemd_nspawn,
    /// OpenVZ / Virtuozzo container.
    openvz,
    /// Windows Subsystem for Linux (WSL).
    wsl,
    /// Appbox container.
    appbox,
    /// Other classified container runtime.
    other
};

/// Classified application sandbox or confinement mechanism.
enum class sandbox_type : std::uint8_t {
    /// Sandbox confinement is detected, but the mechanism is unrecognized.
    unknown,
    /// Execution is not within a detected application sandbox.
    none,
    /// Flatpak application sandbox.
    flatpak,
    /// Canonical Snap package confinement.
    snap,
    /// Apple App Sandbox (macOS seatbelt).
    apple_sandbox,
    /// Microsoft Windows AppContainer / UWP / MSIX sandbox.
    windows_app_container,
    /// Other classified sandbox environment.
    other
};

} // namespace virtualization
} // namespace syscape

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/virtualization/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/virtualization/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__APPLE__) && \
    defined(__MACH__) && !defined(__ENVIRONMENT_IPHONE_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_WATCH_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_TV_OS_VERSION_MIN_REQUIRED__) && \
    !defined(__ENVIRONMENT_VISION_OS_VERSION_MIN_REQUIRED__)
#include <syscape/detail/virtualization/macos.hpp>
#else
#include <syscape/detail/virtualization/generic.hpp>
#endif

namespace syscape {
namespace virtualization {

/// Reports whether the system is executing under a hypervisor or virtual machine.
///
/// @return true if a hypervisor is detected, false for bare metal, or an error.
inline result<bool> is_hypervisor_present() {
    return detail::virtualization_backend::is_hypervisor_present();
}

/// Returns the classified hypervisor vendor.
///
/// @return The hypervisor vendor enum (hypervisor_vendor::none if bare metal),
/// or an error such as not_supported on unsupported platforms.
inline result<hypervisor_vendor> hypervisor() {
    return detail::virtualization_backend::hypervisor();
}

/// Returns the detected hypervisor name or raw signature string as UTF-8.
///
/// @return The hypervisor signature string, not_found when running on bare metal,
/// or an error.
inline result<std::string> hypervisor_name() {
    return detail::virtualization_common::validate_identity_text(
        detail::virtualization_backend::hypervisor_name());
}

/// Reports whether the current process is executing within a container runtime.
///
/// @return true if containerized, false otherwise, or an error.
inline result<bool> is_container() {
    return detail::virtualization_backend::is_container();
}

/// Returns the classified container runtime.
///
/// @return The container runtime enum (container_runtime::none if not containerized),
/// or an error.
inline result<container_runtime> container() {
    return detail::virtualization_backend::container();
}

/// Returns the detected container runtime or environment name as UTF-8.
///
/// @return The container runtime name, not_found when not containerized, or an error.
inline result<std::string> container_name() {
    return detail::virtualization_common::validate_identity_text(
        detail::virtualization_backend::container_name());
}

/// Reports whether the process is executing within Windows Subsystem for Linux (WSL).
///
/// @return true if executing within WSL 1 or WSL 2, false otherwise, or an error.
inline result<bool> is_wsl() {
    return detail::virtualization_backend::is_wsl();
}

/// Returns the major version of Windows Subsystem for Linux (1 or 2).
///
/// @return 1 for WSL 1, 2 for WSL 2, not_found when not running under WSL, or an error.
inline result<std::uint32_t> wsl_version() {
    return detail::virtualization_backend::wsl_version();
}

/// Reports whether the current process is executing within an application sandbox.
///
/// @return true if sandboxed, false otherwise, or an error.
inline result<bool> is_sandboxed() {
    return detail::virtualization_backend::is_sandboxed();
}

/// Returns the classified application sandbox mechanism.
///
/// @return The sandbox type enum (sandbox_type::none if not sandboxed), or an error.
inline result<sandbox_type> sandbox() {
    return detail::virtualization_backend::sandbox();
}

} // namespace virtualization
} // namespace syscape

#endif
