#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_MACOS_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_MACOS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

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

struct sandbox_info {
    bool is_sandboxed = false;
    virtualization_common::sandbox_type type =
        virtualization_common::sandbox_type::none;
};

/// Reads integer sysctl value on macOS.
inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return value;
}

/// Reads string sysctl value on macOS.
inline result<std::string> read_sysctl_string(const char* name) {
    std::size_t size = 0;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0) { return std::string(); }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

/// Helper for synthetic tests to interpret macOS sysctl and model facts.
inline hypervisor_info interpret_macos_hypervisor(
    int hv_vmm_present, std::string_view cpu_features,
    std::string_view model_name) noexcept {
    hypervisor_info info;
    if (hv_vmm_present != 0 ||
        virtualization_common::case_insensitive_contains(cpu_features, "VMM")) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::apple_hypervisor;
        info.name = "Apple Hypervisor";
    }

    if (!model_name.empty()) {
        if (virtualization_common::case_insensitive_contains(model_name, "VMware")) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::vmware;
            info.name = "VMware";
        } else if (virtualization_common::case_insensitive_contains(model_name, "Parallels")) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::parallels;
            info.name = "Parallels";
        } else if (virtualization_common::case_insensitive_contains(model_name, "VirtualBox")) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::virtualbox;
            info.name = "VirtualBox";
        } else if (virtualization_common::case_insensitive_contains(model_name, "QEMU")) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::qemu;
            info.name = "QEMU";
        } else if (virtualization_common::case_insensitive_contains(model_name, "Virtual")) {
            info.present = true;
            if (info.vendor == virtualization_common::hypervisor_type::none) {
                info.vendor = virtualization_common::hypervisor_type::apple_hypervisor;
                info.name = "Apple Virtualization";
            }
        }
    }
    return info;
}

/// Detects hypervisor via Darwin sysctls and IOKit platform expert device.
inline result<hypervisor_info> detect_hypervisor() {
    int vmm_flag = 0;
    const result<int> hv_vmm = read_sysctl_int("kern.hv_vmm_present");
    if (hv_vmm && *hv_vmm != 0) {
        vmm_flag = 1;
    }

    std::string cpu_features;
    const result<std::string> features = read_sysctl_string("machdep.cpu.features");
    if (features) {
        cpu_features = *features;
    }

    std::string model;
#if defined(kIOMainPortDefault)
    const ::io_service_t port = kIOMainPortDefault;
#else
    const ::io_service_t port = kIOMasterPortDefault;
#endif
    ::CFMutableDictionaryRef matching = ::IOServiceMatching("IOPlatformExpertDevice");
    if (matching != nullptr) {
        const ::io_service_t service = ::IOServiceGetMatchingService(port, matching);
        if (service != IO_OBJECT_NULL) {
            const ::CFTypeRef model_prop = ::IORegistryEntryCreateCFProperty(
                service, CFSTR("model"), ::kCFAllocatorDefault, 0);
            if (model_prop != nullptr) {
                if (::CFGetTypeID(model_prop) == ::CFDataGetTypeID()) {
                    const ::CFDataRef data = static_cast<::CFDataRef>(model_prop);
                    const ::CFIndex len = ::CFDataGetLength(data);
                    const ::UInt8* bytes = ::CFDataGetBytePtr(data);
                    if (len > 0 && bytes != nullptr) {
                        model = std::string(reinterpret_cast<const char*>(bytes),
                                            static_cast<std::size_t>(len));
                    }
                }
                ::CFRelease(model_prop);
            }
            ::IOObjectRelease(service);
        }
    }

    return interpret_macos_hypervisor(vmm_flag, cpu_features, model);
}

inline result<container_info> detect_container() {
    container_info info;
    return info;
}

/// Detects macOS App Sandbox via standard environment indicator.
inline result<sandbox_info> detect_sandbox() {
    sandbox_info info;
    if (::getenv("APP_SANDBOX_CONTAINER_ID") != nullptr) {
        info.is_sandboxed = true;
        info.type = virtualization_common::sandbox_type::apple_sandbox;
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
    return false;
}

inline result<std::uint32_t> wsl_version() {
    return fail(errc::not_found);
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
