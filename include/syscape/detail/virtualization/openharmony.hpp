#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_OPENHARMONY_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/openharmony/parameter.hpp>
#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>
#include <syscape/virtualization.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
    const auto qemu = openharmony::get_parameter("ro.kernel.qemu");
    if (qemu && *qemu == "1") {
        return true;
    }
    if (!qemu && qemu.error() != errc::not_found &&
        qemu.error() != errc::not_supported) {
        return fail(qemu.error());
    }

    const auto hw = openharmony::get_parameter("ro.hardware");
    if (hw && (*hw == "goldfish" || *hw == "ranchu" || *hw == "qemu")) {
        return true;
    }
    if (!hw && hw.error() != errc::not_found &&
        hw.error() != errc::not_supported) {
        return fail(hw.error());
    }

    const auto model = openharmony::product_model();
    if (model && (model->find("qemu") != std::string::npos ||
                  model->find("QEMU") != std::string::npos)) {
        return true;
    }
    if (!model && model.error() != errc::not_found &&
        model.error() != errc::not_supported) {
        return fail(model.error());
    }

    return fail(errc::not_supported);
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    const auto qemu = openharmony::get_parameter("ro.kernel.qemu");
    if (qemu && *qemu == "1") {
        return virtualization_common::hypervisor_type::qemu;
    }
    if (!qemu && qemu.error() != errc::not_found &&
        qemu.error() != errc::not_supported) {
        return fail(qemu.error());
    }

    const auto hw = openharmony::get_parameter("ro.hardware");
    if (hw && (*hw == "goldfish" || *hw == "ranchu" || *hw == "qemu")) {
        return virtualization_common::hypervisor_type::qemu;
    }
    if (!hw && hw.error() != errc::not_found &&
        hw.error() != errc::not_supported) {
        return fail(hw.error());
    }

    const auto model = openharmony::product_model();
    if (model && (model->find("qemu") != std::string::npos ||
                  model->find("QEMU") != std::string::npos)) {
        return virtualization_common::hypervisor_type::qemu;
    }
    if (!model && model.error() != errc::not_found &&
        model.error() != errc::not_supported) {
        return fail(model.error());
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
