#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_HAIKU_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_HAIKU_HPP

#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#define SYSCAPE_HAS_HAIKU_OS_H 1
#endif
#endif

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
#if (defined(__x86_64__) || defined(__i386__)) &&                              \
    defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::cpuid_info info {};
    if (::get_cpuid(&info, 1, 0) == B_OK) {
        return (info.regs.ecx & (1U << 31)) != 0;
    }
    return fail(errc::not_supported);
#else
    return fail(errc::not_supported);
#endif
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    auto present = is_hypervisor_present();
    if (!present) {
        return fail(present.error());
    }
    if (!*present) {
        return virtualization_common::hypervisor_type::none;
    }
#if (defined(__x86_64__) || defined(__i386__)) &&                              \
    defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::cpuid_info info {};
    if (::get_cpuid(&info, 0x40000000, 0) == B_OK) {
        char sig[13] = {};
        std::memcpy(sig, &info.regs.ebx, 4);
        std::memcpy(sig + 4, &info.regs.ecx, 4);
        std::memcpy(sig + 8, &info.regs.edx, 4);
        const std::string_view s(sig);
        if (s.find("KVMKVMKVM") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::kvm;
        }
        if (s.find("VMwareVMware") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::vmware;
        }
        if (s.find("Microsoft Hv") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::hyper_v;
        }
        if (s.find("bhyve bhyve ") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::bhyve;
        }
        if (s.find("TCGTCGTCGTCG") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::qemu;
        }
        if (s.find("VBoxVBoxVBox") != std::string_view::npos) {
            return virtualization_common::hypervisor_type::virtualbox;
        }
        return virtualization_common::hypervisor_type::other;
    }
    return virtualization_common::hypervisor_type::unknown;
#else
    return virtualization_common::hypervisor_type::unknown;
#endif
}

inline result<std::string> hypervisor_name() {
    auto present = is_hypervisor_present();
    if (!present) {
        return fail(present.error());
    }
    if (!*present) {
        return fail(errc::not_found);
    }
#if (defined(__x86_64__) || defined(__i386__)) &&                              \
    defined(SYSCAPE_HAS_HAIKU_OS_H)
    ::cpuid_info info {};
    if (::get_cpuid(&info, 0x40000000, 0) == B_OK) {
        char sig[13] = {};
        std::memcpy(sig, &info.regs.ebx, 4);
        std::memcpy(sig + 4, &info.regs.ecx, 4);
        std::memcpy(sig + 8, &info.regs.edx, 4);
        std::string s(sig);
        if (!s.empty()) {
            return s;
        }
    }
#endif
    return fail(errc::not_found);
}

inline result<bool> is_container() {
    return false;
}

inline result<virtualization_common::container_type> container() {
    return virtualization_common::container_type::none;
}

inline result<std::string> container_name() {
    return fail(errc::not_found);
}

inline result<bool> is_wsl() {
    return false;
}

inline result<std::uint32_t> wsl_version() {
    return fail(errc::not_supported);
}

inline result<bool> is_sandboxed() {
    return false;
}

inline result<virtualization_common::sandbox_type> sandbox() {
    return virtualization_common::sandbox_type::none;
}

inline result<std::string> sandbox_name() {
    return fail(errc::not_found);
}

inline result<bool> is_virtual_machine() {
    return is_hypervisor_present();
}

inline result<virtualization_common::cgroup_version_type> cgroup_version() {
    return fail(errc::not_supported);
}

inline result<std::vector<virtualization_common::namespace_category>>
supported_namespaces() {
    return fail(errc::not_supported);
}

inline result<std::vector<virtualization_common::namespace_record>>
active_namespaces() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::cgroup_limits_record> cgroup_limits() {
    return fail(errc::not_supported);
}

inline result<std::vector<virtualization_common::cgroup_record>>
cgroup_controllers() {
    return fail(errc::not_supported);
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
