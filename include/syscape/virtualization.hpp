#ifndef SYSCAPE_VIRTUALIZATION_HPP
#define SYSCAPE_VIRTUALIZATION_HPP

/// @file
/// @brief Hosted virtualization, hypervisor, container, WSL, and sandbox
/// queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms, Android, and OpenHarmony).
/// @note Apple mobile platforms report only affirmative hypervisor or sandbox
/// evidence exposed by public sysctl values or the application-sandbox
/// environment. Queries without definitive evidence, along with container,
/// cgroup, and namespace queries, report not_supported.
/// @note This module exposes:
/// - Hypervisor presence and classified vendor identity (e.g. KVM, QEMU,
/// VMware,
///   Hyper-V, VirtualBox, Xen, bhyve, Parallels, Apple Hypervisor, ACRN, QNX).
/// - Container runtime detection (e.g. Docker, Podman, LXC, LXD, containerd,
///   Kubernetes, systemd-nspawn, OpenVZ, WSL, Appbox).
/// - Windows Subsystem for Linux (WSL) presence and version (WSL 1 vs WSL 2).
/// - Application sandbox detection (e.g. Flatpak, Snap, Apple App Sandbox,
///   Windows AppContainer).
/// - Cgroup hierarchy version (v1, v2, hybrid), process cgroup path, enabled
///   controllers, and active resource limits (memory.max, cpu.max, pids.max).
/// - Linux namespace enumeration, inode IDs, and isolation classification
///   (PID, Mount, Net, User, IPC, UTS, Cgroup, Time).
/// @note Linux implements hypervisor queries through CPUID instruction leaves
/// (leaf 1 ECX hypervisor bit and leaf 0x40000000 signature), DMI sysfs
/// attributes under /sys/class/dmi/id, and /sys/hypervisor/type. Container
/// queries inspect /run/systemd/container, /.dockerenv, /.containerenv, cgroup
/// path hierarchies, and /proc/vz. WSL queries inspect WSL interop endpoints
/// and kernel release strings. Sandbox queries inspect Flatpak and Snap
/// environment indicators. Cgroup queries inspect /proc/self/cgroup and
/// /sys/fs/cgroup. Namespace queries inspect /proc/self/ns/* symlinks and
/// /proc/1/ns/* root references.
/// @note Windows implements hypervisor queries through CPUID instruction leaves
/// and raw SMBIOS table inspection. Sandbox queries inspect process token
/// AppContainer classifications. Cgroup and namespace queries return
/// not_supported.
/// @note macOS implements hypervisor queries through sysctl
/// kern.hv_vmm_present, machdep.cpu.features VMM flags, and IOKit platform
/// expert device matching. Sandbox queries inspect the Apple sandbox
/// environment. Cgroup and namespace queries return not_supported.
/// @note Solaris implements container queries through getzoneid() and
/// getzonenamebyid().
/// @note Haiku implements hypervisor detection through CPUID hypervisor leaves
/// (0x1 and 0x40000000); container, sandbox, cgroup, and namespace queries
/// report none, false, or not_supported.
/// @note AIX hypervisor, container, sandbox, cgroup, and namespace queries
/// report none, false, or not_supported.
/// @note HP-UX hypervisor, container, sandbox, cgroup, and namespace queries
/// report not_supported.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/virtualization.hpp requires C++17 or later"
#endif

#include <syscape/detail/utf8.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

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

/// Classified cgroup hierarchy version.
enum class cgroup_version : std::uint8_t {
    /// Cgroup hierarchy is not present, unmounted, or unsupported.
    none,
    /// Legacy cgroup v1 multi-hierarchy.
    v1,
    /// Unified cgroup v2 single hierarchy.
    v2,
    /// Hybrid cgroup v1 and v2 hierarchy.
    hybrid
};

/// Linux namespace category.
enum class namespace_type : std::uint8_t {
    /// Unrecognized or unspecified namespace category.
    unknown,
    /// Cgroup namespace (CLONE_NEWCGROUP).
    cgroup,
    /// IPC namespace (CLONE_NEWIPC).
    ipc,
    /// Mount / filesystem namespace (CLONE_NEWNS).
    mount,
    /// Network namespace (CLONE_NEWNET).
    net,
    /// Process ID namespace (CLONE_NEWPID).
    pid,
    /// Process ID namespace for child processes.
    pid_for_children,
    /// Time namespace (CLONE_NEWTIME).
    time,
    /// Time namespace for child processes.
    time_for_children,
    /// User namespace (CLONE_NEWUSER).
    user,
    /// UTS / hostname namespace (CLONE_NEWUTS).
    uts
};

/// Metadata describing a single namespace membership of the calling process.
struct namespace_info {
    /// Namespace category.
    namespace_type type = namespace_type::unknown;

    /// Platform-specific namespace name (e.g. "net", "pid", "mnt").
    std::string name;

    /// Inode identifier of the namespace (from /proc/self/ns/* or stat).
    std::uint64_t inode = 0U;

    /// Device ID of the namespace filesystem / proc mount.
    std::uint64_t device_id = 0U;

    /// Indicates whether this namespace is isolated from the root / init namespace.
    /// std::nullopt if the init namespace cannot be observed (e.g. unprivileged user).
    std::optional<bool> is_isolated;
};

/// Resource limits configured by the controlling cgroup.
struct cgroup_limits {
    /// Maximum memory limit in bytes (std::nullopt if unconstrained or "max").
    std::optional<std::uint64_t> memory_max_bytes;

    /// High memory throttle threshold in bytes (std::nullopt if unconstrained or "max").
    std::optional<std::uint64_t> memory_high_bytes;

    /// Maximum swap usage limit in bytes (std::nullopt if unconstrained or "max").
    std::optional<std::uint64_t> memory_swap_max_bytes;

    /// CPU quota in microseconds per period (std::nullopt if unconstrained or "max").
    std::optional<std::uint64_t> cpu_quota_us;

    /// CPU scheduling period in microseconds, if configured.
    std::optional<std::uint64_t> cpu_period_us;

    /// Maximum number of process IDs / tasks allowed in the cgroup (std::nullopt if "max").
    std::optional<std::uint64_t> pids_max;
};

/// Information describing the active cgroup hierarchy and membership for the current process.
struct cgroup_info {
    /// Hierarchy version (v1, v2, hybrid, or none).
    cgroup_version version = cgroup_version::none;

    /// Relative cgroup path of the process within the unified or primary hierarchy (e.g. "/user.slice/...").
    std::string path;

    /// Configured resource constraints for this cgroup.
    cgroup_limits limits;

    /// List of enabled cgroup controllers (e.g. "cpu", "memory", "pids", "io").
    std::vector<std::string> controllers;
};

} // namespace virtualization
} // namespace syscape

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) &&           \
    !defined(__ANDROID__) && !defined(SYSCAPE_TARGET_OPENHARMONY) &&           \
    !defined(SYSCAPE_TARGET_AIX) && !defined(SYSCAPE_TARGET_HPUX) &&           \
    !defined(SYSCAPE_TARGET_HURD) && !defined(SYSCAPE_TARGET_SERENITY)
#include <syscape/detail/virtualization/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/virtualization/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_APPLE_MOBILE)
#include <syscape/detail/virtualization/apple_mobile.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/virtualization/macos.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__FreeBSD__)
#include <syscape/detail/virtualization/freebsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__OpenBSD__)
#include <syscape/detail/virtualization/openbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__NetBSD__)
#include <syscape/detail/virtualization/netbsd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__DragonFly__)
#include <syscape/detail/virtualization/dragonfly.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__ANDROID__)
#include <syscape/detail/virtualization/android.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_OPENHARMONY)
#include <syscape/detail/virtualization/openharmony.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    (defined(__sun) || defined(__sun__) || defined(sun))
#include <syscape/detail/virtualization/solaris.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__HAIKU__)
#include <syscape/detail/virtualization/haiku.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_AIX)
#include <syscape/detail/virtualization/aix.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HPUX)
#include <syscape/detail/virtualization/hpux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_HURD)
#include <syscape/detail/virtualization/hurd.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) &&                               \
    defined(SYSCAPE_TARGET_SERENITY)
#include <syscape/detail/virtualization/serenity.hpp>
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

/// Returns the detected cgroup hierarchy version.
///
/// @return The cgroup_version enum, or an error if unsupported or inaccessible.
inline result<cgroup_version> cgroup_hierarchy_version() {
    return detail::virtualization_backend::cgroup_hierarchy_version();
}

/// Returns the full cgroup configuration and active resource limits for the calling process.
///
/// @return The cgroup_info structure, or an error if unsupported or inaccessible.
inline result<cgroup_info> current_cgroup() {
    auto res = detail::virtualization_backend::current_cgroup();
    if (!res) {
        return fail(res.error());
    }
    if (!detail::is_valid_utf8(res->path)) {
        return fail(errc::invalid_encoding);
    }
    for (const auto& ctrl : res->controllers) {
        if (!detail::is_valid_utf8(ctrl)) {
            return fail(errc::invalid_encoding);
        }
    }
    return res;
}

/// Enumerates all active namespace memberships and isolation statuses for the calling process.
///
/// @return A vector of namespace_info structures, or an error.
inline result<std::vector<namespace_info>> namespaces() {
    auto res = detail::virtualization_backend::namespaces();
    if (!res) {
        return fail(res.error());
    }
    for (const auto& ns : *res) {
        if (!detail::is_valid_utf8(ns.name)) {
            return fail(errc::invalid_encoding);
        }
    }
    return res;
}

/// Checks whether the calling process is running in any isolated namespace relative to PID 1.
///
/// @return true if running in an isolated namespace, false if in root namespaces, or an error.
inline result<bool> is_namespace_isolated() {
    return detail::virtualization_backend::is_namespace_isolated();
}

} // namespace virtualization
} // namespace syscape

#endif
