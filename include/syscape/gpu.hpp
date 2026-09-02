#ifndef SYSCAPE_GPU_HPP
#define SYSCAPE_GPU_HPP

/// @file
/// @brief Hosted GPU, graphics adapter, and video controller queries.
/// @note Minimum compatibility profile: Hosted Full with C++17
/// (Sandboxed/Restricted on Apple mobile platforms and Android).
/// @note Apple mobile platforms expose no permitted public GPU inventory
/// source to this C++ interface, so all queries report not_supported.
/// @note This module exposes:
/// - Enumeration of installed GPU devices (devices()) and adapter counts
/// (device_count()).
/// - Identification of the primary boot / display adapter (primary_device()).
/// - Classified vendor identification (syscape::gpu::gpu_vendor).
/// - Classified vendor names, model descriptions, and driver names.
/// - PCI vendor ID and device ID where available.
/// - Dedicated video memory (VRAM) capacity in bytes where exposed by the
/// OS/driver.
/// @note Linux queries sysfs DRM interfaces (/sys/class/drm) and PCI devices
/// (/sys/bus/pci/devices).
/// @note Windows queries Win32 display device interfaces.
/// @note macOS queries IOKit registry classes (IOPCIDevice, IOAccelerator).

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/gpu.hpp requires C++17 or later"
#endif

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace syscape {
namespace gpu {

/// Classified graphics hardware vendor or manufacturer.
enum class gpu_vendor : std::uint8_t {
    /// The vendor could not be determined.
    unknown,
    /// Advanced Micro Devices (AMD / ATI).
    amd,
    /// NVIDIA Corporation.
    nvidia,
    /// Intel Corporation.
    intel,
    /// Apple Inc. (Apple Silicon GPU).
    apple,
    /// ARM Mali / Immortalis GPU.
    arm_mali,
    /// Qualcomm Adreno GPU.
    qualcomm_adreno,
    /// Broadcom VideoCore GPU (e.g. Raspberry Pi).
    broadcom_videocore,
    /// Imagination Technologies PowerVR GPU.
    imagination_powervr,
    /// Microsoft Corporation (Basic Render Driver / Hyper-V Video).
    microsoft,
    /// VMware SVGA virtual graphics adapter.
    vmware,
    /// Red Hat / VirtIO virtual GPU.
    virtio,
    /// Other vendor not individually cataloged.
    other
};

/// Information describing a single graphics processing unit or display adapter.
struct gpu_device {
    /// Platform-specific device identifier or path (e.g. "0000:01:00.0", "card0", or DXGI LUID).
    std::string id;

    /// Device name, model description, or product label as UTF-8, if exposed.
    std::optional<std::string> name;

    /// Classified vendor enum.
    gpu_vendor vendor = gpu_vendor::unknown;

    /// Classified or platform-provided vendor name as UTF-8.
    std::string vendor_name;

    /// Numeric PCI vendor ID (e.g. 0x10de for NVIDIA, 0x1002 for AMD, 0x8086 for Intel), if exposed.
    std::optional<std::uint32_t> vendor_id;

    /// Numeric PCI device ID, if exposed.
    std::optional<std::uint32_t> device_id;

    /// Kernel/OS driver or driver module name (e.g. "nvidia", "amdgpu", "i915"), if available.
    std::optional<std::string> driver;

    /// Dedicated video memory (VRAM) in bytes, if exposed by the platform/driver.
    std::optional<std::uint64_t> vram_bytes;

    /// Indicates whether this GPU is the primary boot / display adapter, if determinable.
    std::optional<bool> is_primary;
};

} // namespace gpu
} // namespace syscape

#include <syscape/detail/gpu/common.hpp>
#include <syscape/result.hpp>

#if !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(__linux__) && \
    !defined(__ANDROID__)
#include <syscape/detail/gpu/linux.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(_WIN32)
#include <syscape/detail/gpu/windows.hpp>
#elif !defined(SYSCAPE_FORCE_GENERIC_BACKEND) && defined(SYSCAPE_TARGET_MACOS)
#include <syscape/detail/gpu/macos.hpp>
#else
#include <syscape/detail/gpu/generic.hpp>
#endif

namespace syscape {
namespace gpu {

/// Enumerates all detected GPU devices and graphics adapters on the system.
///
/// @return A vector of gpu_device structures, an empty vector if no GPUs are detected,
/// or an error.
inline result<std::vector<gpu_device>> devices() {
    return detail::gpu_backend::devices();
}

/// Returns the number of detected GPU devices and graphics adapters on the system.
///
/// @return The count of GPU adapters, or an error.
inline result<std::size_t> device_count() {
    return detail::gpu_backend::device_count();
}

/// Returns the primary boot or display adapter.
///
/// @return The primary gpu_device, not_found if no primary device is identified,
/// or an error.
inline result<gpu_device> primary_device() {
    return detail::gpu_backend::primary_device();
}

} // namespace gpu
} // namespace syscape

#endif
