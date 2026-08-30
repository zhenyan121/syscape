#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_FREEBSD_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_FREEBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/virtualization.hpp>
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

inline result<hypervisor_info> detect_hypervisor() {
    hypervisor_info info;
    std::size_t size = 0U;
    if (::sysctlbyname("kern.vm_guest", nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return info;
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return info;
    }

    std::string guest(size, '\0');
    if (::sysctlbyname("kern.vm_guest", &guest[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!guest.empty() && (guest.back() == '\0' || guest.back() == '\n')) {
        guest.pop_back();
    }

    if (!guest.empty() && guest != "none") {
        info.present = true;
        if (guest == "generic") {
            info.vendor = virtualization_common::hypervisor_type::unknown;
            info.name = "Generic Hypervisor";
        } else if (guest == "xen") {
            info.vendor = virtualization_common::hypervisor_type::xen;
            info.name = "Xen";
        } else if (guest == "hv" || guest == "hyper-v" || guest == "hyperv") {
            info.vendor = virtualization_common::hypervisor_type::hyper_v;
            info.name = "Hyper-V";
        } else if (guest == "vmware") {
            info.vendor = virtualization_common::hypervisor_type::vmware;
            info.name = "VMware";
        } else if (guest == "kvm") {
            info.vendor = virtualization_common::hypervisor_type::kvm;
            info.name = "KVM";
        } else if (guest == "bhyve") {
            info.vendor = virtualization_common::hypervisor_type::bhyve;
            info.name = "bhyve";
        } else if (guest == "vbox" || guest == "virtualbox") {
            info.vendor = virtualization_common::hypervisor_type::virtualbox;
            info.name = "VirtualBox";
        } else if (guest == "parallels") {
            info.vendor = virtualization_common::hypervisor_type::parallels;
            info.name = "Parallels";
        } else if (guest == "qemu") {
            info.vendor = virtualization_common::hypervisor_type::qemu;
            info.name = "QEMU";
        } else {
            info.vendor = virtualization_common::hypervisor_type::other;
            info.name = guest;
        }
    }
    return info;
}

inline result<container_info> detect_container() {
    container_info info;
    int jailed = 0;
    std::size_t size = sizeof(jailed);
    if (::sysctlbyname("security.jail.jailed", &jailed, &size, nullptr, 0U) ==
            0 &&
        jailed != 0) {
        info.present = true;
        info.runtime = virtualization_common::container_type::other;
        info.name = "FreeBSD Jail";
    }
    return info;
}

inline result<sandbox_info> detect_sandbox() {
    sandbox_info info;
    return info;
}

inline result<bool> is_hypervisor_present() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) {
        return fail(info.error());
    }
    return info->present;
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) {
        return fail(info.error());
    }
    return info->vendor;
}

inline result<std::string> hypervisor_name() {
    const result<hypervisor_info> info = detect_hypervisor();
    if (!info) {
        return fail(info.error());
    }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_container() {
    const result<container_info> info = detect_container();
    if (!info) {
        return fail(info.error());
    }
    return info->present;
}

inline result<virtualization_common::container_type> container() {
    const result<container_info> info = detect_container();
    if (!info) {
        return fail(info.error());
    }
    return info->runtime;
}

inline result<std::string> container_name() {
    const result<container_info> info = detect_container();
    if (!info) {
        return fail(info.error());
    }
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
    if (!info) {
        return fail(info.error());
    }
    return info->is_sandboxed;
}

inline result<virtualization_common::sandbox_type> sandbox() {
    const result<sandbox_info> info = detect_sandbox();
    if (!info) {
        return fail(info.error());
    }
    return info->type;
}

inline result<virtualization_common::cgroup_version_type>
cgroup_hierarchy_version() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::cgroup_record> current_cgroup() {
    return fail(errc::not_supported);
}

inline result<std::vector<virtualization_common::namespace_record>>
namespaces() {
    return fail(errc::not_supported);
}

inline result<bool> is_namespace_isolated() {
    return fail(errc::not_supported);
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
