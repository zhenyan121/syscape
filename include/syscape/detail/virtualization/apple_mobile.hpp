#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_APPLE_MOBILE_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_APPLE_MOBILE_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>
#include <sys/sysctl.h>

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

inline result<int> read_sysctl_int(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<int>(fail(errc::not_supported))
                   : result<int>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    if (size != sizeof(value)) {
        return fail(errc::malformed_data);
    }
    return value;
}

inline result<std::string> read_sysctl_string(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<std::string>(fail(errc::not_supported))
                   : result<std::string>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    if (size == 0U) {
        return std::string();
    }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    while (!value.empty() && (value.back() == '\0' || value.back() == '\n')) {
        value.pop_back();
    }
    return value;
}

inline result<hypervisor_info> query_hypervisor_info() {
    hypervisor_info info;
    const result<int> hv_vmm = read_sysctl_int("kern.hv_vmm_present");
    if (hv_vmm) {
        if (*hv_vmm != 0) {
            info.present = true;
            info.vendor =
                virtualization_common::hypervisor_type::apple_hypervisor;
            info.name = "Apple Hypervisor";
            return info;
        }
    } else if (hv_vmm.error() != errc::not_supported) {
        return fail(hv_vmm.error());
    }

    const result<std::string> model = read_sysctl_string("hw.model");
    if (model) {
        if (!model->empty()) {
            if (virtualization_common::case_insensitive_contains(*model,
                                                                 "QEMU")) {
                info.present = true;
                info.vendor = virtualization_common::hypervisor_type::qemu;
                info.name = "QEMU";
                return info;
            }
            if (virtualization_common::case_insensitive_contains(
                    *model, "VirtualBox")) {
                info.present = true;
                info.vendor =
                    virtualization_common::hypervisor_type::virtualbox;
                info.name = "VirtualBox";
                return info;
            }
        }
    } else if (model.error() != errc::not_supported) {
        return fail(model.error());
    }

    // Without affirmative positive evidence from kernel/hardware sysctls,
    // Apple Mobile environments cannot definitively prove bare-metal status.
    return fail(errc::not_supported);
}

inline result<bool> is_hypervisor_present() {
    const auto info = query_hypervisor_info();
    if (!info) {
        return fail(info.error());
    }
    return info->present;
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const auto info = query_hypervisor_info();
    if (!info) {
        return fail(info.error());
    }
    return info->vendor;
}

inline result<std::string> hypervisor_name() {
    const auto info = query_hypervisor_info();
    if (!info) {
        return fail(info.error());
    }
    if (!info->present || info->name.empty()) {
        return fail(errc::not_found);
    }
    return info->name;
}

inline result<bool> is_container() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::container_type> container() {
    return fail(errc::not_supported);
}

inline result<std::string> container_name() {
    return fail(errc::not_supported);
}

inline result<bool> is_wsl() {
    return false;
}

inline result<std::uint32_t> wsl_version() {
    return fail(errc::not_found);
}

inline result<bool> is_sandboxed() {
    const char* container_id = ::getenv("APP_SANDBOX_CONTAINER_ID");
    if (container_id != nullptr && container_id[0] != '\0') {
        return true;
    }
    return fail(errc::not_supported);
}

inline result<virtualization_common::sandbox_type> sandbox() {
    const char* container_id = ::getenv("APP_SANDBOX_CONTAINER_ID");
    if (container_id != nullptr && container_id[0] != '\0') {
        return virtualization_common::sandbox_type::apple_sandbox;
    }
    return fail(errc::not_supported);
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
