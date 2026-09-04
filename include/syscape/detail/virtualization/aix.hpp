#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_AIX_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_AIX_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#if defined(__has_include)
#if __has_include(<sys/systemcfg.h>)
#include <sys/systemcfg.h>
#define SYSCAPE_HAS_AIX_SYSTEMCFG 1
#endif
#endif

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    return virtualization_common::hypervisor_type::unknown;
}

inline result<std::string> hypervisor_name() {
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
