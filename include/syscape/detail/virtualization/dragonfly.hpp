#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_DRAGONFLY_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <kenv.h>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/virtualization.hpp>
#include <syscape/detail/utf8.hpp>
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

inline result<std::string> read_sysctl_by_name(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT || errno == EINVAL || errno == EOPNOTSUPP ||
            errno == ENODEV || errno == ENOTSUP) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        if (errno == ENOENT || errno == EINVAL || errno == EOPNOTSUPP ||
            errno == ENODEV || errno == ENOTSUP) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r' || value.back() == ' ')) {
        value.pop_back();
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    if (value.empty()) {
        return fail(errc::not_found);
    }
    return value;
}

inline result<std::string> read_kenv_value(const char* name) {
    char buffer[256] = {};
    const int ret =
        ::kenv(KENV_GET, name, buffer, static_cast<int>(sizeof(buffer)));
    if (ret < 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    std::string value(buffer);
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r' || value.back() == ' ')) {
        value.pop_back();
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    if (value.empty()) {
        return fail(errc::not_found);
    }
    return value;
}

inline std::string to_lower_ascii(std::string_view s) {
    std::string result;
    result.reserve(s.size());
    for (char c : s) {
        result.push_back(
            static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return result;
}

inline result<hypervisor_info> detect_hypervisor() {
    hypervisor_info info;

    // Check kern.vmm_guest or kern.vm_guest
    auto guest_res = read_sysctl_by_name("kern.vmm_guest");
    if (!guest_res || guest_res->empty() || *guest_res == "none") {
        guest_res = read_sysctl_by_name("kern.vm_guest");
    }
    if (guest_res && !guest_res->empty() && *guest_res != "none") {
        info.present = true;
        const std::string lower = to_lower_ascii(*guest_res);
        if (lower == "generic") {
            info.vendor = virtualization_common::hypervisor_type::unknown;
            info.name = "Generic Hypervisor";
        } else if (lower == "xen") {
            info.vendor = virtualization_common::hypervisor_type::xen;
            info.name = "Xen";
        } else if (lower == "hv" || lower == "hyper-v" || lower == "hyperv") {
            info.vendor = virtualization_common::hypervisor_type::hyper_v;
            info.name = "Hyper-V";
        } else if (lower == "vmware") {
            info.vendor = virtualization_common::hypervisor_type::vmware;
            info.name = "VMware";
        } else if (lower == "kvm") {
            info.vendor = virtualization_common::hypervisor_type::kvm;
            info.name = "KVM";
        } else if (lower == "bhyve") {
            info.vendor = virtualization_common::hypervisor_type::bhyve;
            info.name = "bhyve";
        } else if (lower == "vbox" || lower == "virtualbox") {
            info.vendor = virtualization_common::hypervisor_type::virtualbox;
            info.name = "VirtualBox";
        } else if (lower == "parallels") {
            info.vendor = virtualization_common::hypervisor_type::parallels;
            info.name = "Parallels";
        } else if (lower == "qemu") {
            info.vendor = virtualization_common::hypervisor_type::qemu;
            info.name = "QEMU";
        } else {
            info.vendor = virtualization_common::hypervisor_type::other;
            info.name = *guest_res;
        }
        return info;
    }

    // Check SMBIOS / DMI hints from sysctl or kenv
    const char* dmi_sysctl_nodes[] = {"machdep.dmi.system-product",
                                      "machdep.dmi.system-vendor",
                                      "machdep.dmi.bios-vendor"};

    const char* smbios_kenv_keys[] = {
        "smbios.system.product", "smbios.system.maker", "smbios.bios.vendor"};

    auto check_hint_string = [&](const std::string& val) -> bool {
        const std::string lower = to_lower_ascii(val);
        if (lower.find("qemu") != std::string::npos ||
            lower.find("bochs") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::qemu;
            info.name = "QEMU";
            return true;
        }
        if (lower.find("virtualbox") != std::string::npos ||
            lower.find("innotek") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::virtualbox;
            info.name = "VirtualBox";
            return true;
        }
        if (lower.find("vmware") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::vmware;
            info.name = "VMware";
            return true;
        }
        if (lower.find("kvm") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::kvm;
            info.name = "KVM";
            return true;
        }
        if (lower.find("bhyve") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::bhyve;
            info.name = "bhyve";
            return true;
        }
        if (lower.find("xen") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::xen;
            info.name = "Xen";
            return true;
        }
        if (lower.find("hyper-v") != std::string::npos) {
            info.present = true;
            info.vendor = virtualization_common::hypervisor_type::hyper_v;
            info.name = "Hyper-V";
            return true;
        }
        return false;
    };

    for (const char* node : dmi_sysctl_nodes) {
        auto val = read_sysctl_by_name(node);
        if (val && check_hint_string(*val)) {
            return info;
        }
    }

    for (const char* key : smbios_kenv_keys) {
        auto val = read_kenv_value(key);
        if (val && check_hint_string(*val)) {
            return info;
        }
    }

    return info;
}

inline result<container_info> detect_container() {
    container_info info;
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
