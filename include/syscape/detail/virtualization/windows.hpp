#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_WINDOWS_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_WINDOWS_HPP

#include <cstdint>
#include <string>
#include <system_error>

#include <windows.h>
#if defined(_MSC_VER) || defined(__INTEL_COMPILER)
#include <intrin.h>
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#endif

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

/// Helper to interpret CPUID registers for hypervisor detection.
inline hypervisor_info parse_cpuid_hypervisor(
    std::uint32_t ecx_leaf1, std::uint32_t ebx_leaf40,
    std::uint32_t ecx_leaf40, std::uint32_t edx_leaf40) noexcept {
    hypervisor_info info;
    constexpr std::uint32_t hypervisor_bit = 1U << 31U;
    if ((ecx_leaf1 & hypervisor_bit) != 0U) {
        info.present = true;
        info.name = virtualization_common::decode_cpuid_signature(
            ebx_leaf40, ecx_leaf40, edx_leaf40);
        info.vendor =
            virtualization_common::classify_cpuid_signature(info.name);
    }
    return info;
}

/// Probes CPUID instruction for hypervisor status on Windows.
inline hypervisor_info probe_cpuid_hypervisor() noexcept {
    hypervisor_info info;
#if (defined(_MSC_VER) || defined(__INTEL_COMPILER)) && \
    (defined(_M_IX86) || defined(_M_X64))
    int cpu_info[4] = {0, 0, 0, 0};
    __cpuid(cpu_info, 1);
    constexpr std::uint32_t hypervisor_bit = 1U << 31U;
    if ((static_cast<std::uint32_t>(cpu_info[2]) & hypervisor_bit) != 0U) {
        info.present = true;
        int leaf40[4] = {0, 0, 0, 0};
        __cpuid(leaf40, static_cast<int>(0x40000000U));
        info.name = virtualization_common::decode_cpuid_signature(
            static_cast<std::uint32_t>(leaf40[1]),
            static_cast<std::uint32_t>(leaf40[2]),
            static_cast<std::uint32_t>(leaf40[3]));
        info.vendor =
            virtualization_common::classify_cpuid_signature(info.name);
    }
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
    unsigned int eax = 0U;
    unsigned int ebx = 0U;
    unsigned int ecx = 0U;
    unsigned int edx = 0U;
    __cpuid(1U, eax, ebx, ecx, edx);
    constexpr unsigned int hypervisor_bit = 1U << 31U;
    if ((ecx & hypervisor_bit) != 0U) {
        info.present = true;
        __cpuid(0x40000000U, eax, ebx, ecx, edx);
        info.name = virtualization_common::decode_cpuid_signature(
            ebx, ecx, edx);
        info.vendor =
            virtualization_common::classify_cpuid_signature(info.name);
    }
#endif
    return info;
}

inline result<hypervisor_info> detect_hypervisor() {
    return probe_cpuid_hypervisor();
}

inline result<container_info> detect_container() {
    // Windows containers detection
    container_info info;
    return info;
}

/// Probes Windows process token for AppContainer sandbox classification.
inline result<sandbox_info> detect_sandbox() {
    sandbox_info info;
    ::HANDLE token = nullptr;
    if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &token)) {
        return fail(std::error_code(
            static_cast<int>(::GetLastError()), std::system_category()));
    }

    struct token_closer {
        ::HANDLE handle;
        ~token_closer() { if (handle) { ::CloseHandle(handle); } }
    } closer{token};

    ::DWORD is_app_container = 0U;
    ::DWORD return_length = 0U;
    if (::GetTokenInformation(token,
                              static_cast<::TOKEN_INFORMATION_CLASS>(29), // TokenIsAppContainer
                              &is_app_container,
                              sizeof(is_app_container),
                              &return_length)) {
        if (is_app_container != 0U) {
            info.is_sandboxed = true;
            info.type = virtualization_common::sandbox_type::windows_app_container;
        }
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
    // Windows host itself is not WSL.
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
