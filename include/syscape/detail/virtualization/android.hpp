#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_ANDROID_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_ANDROID_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/android/property.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>
#include <syscape/virtualization.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
    const auto qemu = android::get_property("ro.kernel.qemu");
    if (qemu && *qemu == "1") {
        return true;
    }
    const auto hw = android::get_property("ro.hardware");
    if (hw && (*hw == "goldfish" || *hw == "ranchu" || *hw == "cuttlefish")) {
        return true;
    }
    const auto boot_hw = android::get_property("ro.boot.hardware");
    if (boot_hw && (*boot_hw == "goldfish" || *boot_hw == "ranchu" ||
                    *boot_hw == "cuttlefish")) {
        return true;
    }
    return fail(errc::not_supported);
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const auto qemu = android::get_property("ro.kernel.qemu");
    if (qemu && *qemu == "1") {
        return virtualization_common::hypervisor_type::qemu;
    }
    const auto hw = android::get_property("ro.hardware");
    if (hw) {
        if (*hw == "goldfish" || *hw == "ranchu") {
            return virtualization_common::hypervisor_type::qemu;
        }
        if (*hw == "cuttlefish") {
            return virtualization_common::hypervisor_type::kvm;
        }
    }
    const auto boot_hw = android::get_property("ro.boot.hardware");
    if (boot_hw) {
        if (*boot_hw == "goldfish" || *boot_hw == "ranchu") {
            return virtualization_common::hypervisor_type::qemu;
        }
        if (*boot_hw == "cuttlefish") {
            return virtualization_common::hypervisor_type::kvm;
        }
    }
    return fail(errc::not_supported);
}

inline result<std::string> hypervisor_name() {
    const auto h = hypervisor();
    if (!h) {
        return fail(h.error());
    }
    if (*h == virtualization_common::hypervisor_type::qemu) {
        return std::string("QEMU");
    }
    if (*h == virtualization_common::hypervisor_type::kvm) {
        return std::string("KVM");
    }
    return fail(errc::not_found);
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
    return fail(errc::not_supported);
}

inline result<virtualization_common::sandbox_type> sandbox() {
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
