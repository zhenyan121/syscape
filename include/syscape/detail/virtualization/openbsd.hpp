#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_OPENBSD_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_OPENBSD_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <sys/types.h>
#include <sys/sysctl.h>
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

inline result<std::string> read_mib_string(const int* mib,
                                           unsigned int mib_len) {
    std::size_t size = 0U;
    int mib_copy[8];
    if (mib_len > 8U) {
        return fail(errc::not_supported);
    }
    for (unsigned int i = 0; i < mib_len; ++i) {
        mib_copy[i] = mib[i];
    }
    if (::sysctl(mib_copy, mib_len, nullptr, &size, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_found);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size == 0U) {
        return fail(errc::not_found);
    }
    std::string value(size, '\0');
    if (::sysctl(mib_copy, mib_len, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n' ||
                              value.back() == '\r' || value.back() == ' ')) {
        value.pop_back();
    }
    if (value.empty()) {
        return fail(errc::not_found);
    }
    return value;
}

inline result<hypervisor_info> detect_hypervisor() {
    hypervisor_info info;
    std::string candidate;

#ifdef HW_PRODUCT
    int mib_prod[] = {CTL_HW, HW_PRODUCT};
    auto prod = read_mib_string(mib_prod, 2U);
    if (!prod && prod.error() != errc::not_found &&
        prod.error() != errc::not_supported) {
        return fail(prod.error());
    }
    if (prod) {
        candidate = *prod;
    }
#endif
#ifdef HW_VENDOR
    int mib_vendor[] = {CTL_HW, HW_VENDOR};
    auto vendor = read_mib_string(mib_vendor, 2U);
    if (!vendor && vendor.error() != errc::not_found &&
        vendor.error() != errc::not_supported) {
        return fail(vendor.error());
    }
    if (vendor && !vendor->empty()) {
        if (!candidate.empty()) {
            candidate += " ";
        }
        candidate += *vendor;
    }
#endif

    std::string lower = candidate;
    std::transform(
        lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower.find("qemu") != std::string::npos ||
        lower.find("bochs") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::qemu;
        info.name = "QEMU";
    } else if (lower.find("vmware") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::vmware;
        info.name = "VMware";
    } else if (lower.find("virtualbox") != std::string::npos ||
               lower.find("innotek") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::virtualbox;
        info.name = "VirtualBox";
    } else if (lower.find("kvm") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::kvm;
        info.name = "KVM";
    } else if (lower.find("bhyve") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::bhyve;
        info.name = "bhyve";
    } else if (lower.find("hyper-v") != std::string::npos ||
               lower.find("virtual machine") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::hyper_v;
        info.name = "Hyper-V";
    } else if (lower.find("xen") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::xen;
        info.name = "Xen";
    } else if (lower.find("openbsd vmm") != std::string::npos) {
        info.present = true;
        info.vendor = virtualization_common::hypervisor_type::other;
        info.name = "OpenBSD VMM";
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
