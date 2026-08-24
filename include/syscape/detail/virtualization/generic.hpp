#ifndef SYSCAPE_DETAIL_VIRTUALIZATION_GENERIC_HPP
#define SYSCAPE_DETAIL_VIRTUALIZATION_GENERIC_HPP

#include <cstdint>
#include <string>

#include <syscape/detail/virtualization/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace virtualization_backend {

inline result<bool> is_hypervisor_present() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::hypervisor_type> hypervisor() {
    return fail(errc::not_supported);
}

inline result<std::string> hypervisor_name() {
    return fail(errc::not_supported);
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
    return fail(errc::not_supported);
}

inline result<std::uint32_t> wsl_version() {
    return fail(errc::not_supported);
}

inline result<bool> is_sandboxed() {
    return fail(errc::not_supported);
}

inline result<virtualization_common::sandbox_type> sandbox() {
    return fail(errc::not_supported);
}

} // namespace virtualization_backend
} // namespace detail
} // namespace syscape

#endif
