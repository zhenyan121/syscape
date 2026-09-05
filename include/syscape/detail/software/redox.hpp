#ifndef SYSCAPE_DETAIL_SOFTWARE_REDOX_HPP
#define SYSCAPE_DETAIL_SOFTWARE_REDOX_HPP

#include <string_view>
#include <vector>

#include <syscape/detail/software/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace software_backend {

inline result<std::vector<software_common::service_record>> services() {
    return fail(errc::not_supported);
}

inline result<software_common::service_record> find_service(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::driver_record>> loaded_drivers() {
    return fail(errc::not_supported);
}

inline result<software_common::driver_record> find_driver(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::package_record>>
installed_packages() {
    return fail(errc::not_supported);
}

inline result<software_common::package_record> find_package(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::update_record>> system_updates() {
    return fail(errc::not_supported);
}

inline result<std::vector<software_common::runtime_record>>
installed_runtimes() {
    return fail(errc::not_supported);
}

} // namespace software_backend
} // namespace detail
} // namespace syscape

#endif
