#ifndef SYSCAPE_DETAIL_WIFI_GENERIC_HPP
#define SYSCAPE_DETAIL_WIFI_GENERIC_HPP

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

#include <syscape/error.hpp>
#include <syscape/result.hpp>
#include <syscape/wifi.hpp>

namespace syscape {
namespace detail {
namespace wifi_backend {

inline result<std::vector<wifi::adapter_info>> adapters() {
    return fail(errc::not_supported);
}

inline result<std::size_t> adapter_count() {
    return fail(errc::not_supported);
}

inline result<wifi::adapter_info> default_adapter() {
    return fail(errc::not_supported);
}

inline result<std::optional<wifi::network_connection>>
current_connection(std::string_view) {
    return fail(errc::not_supported);
}

inline result<std::vector<wifi::configured_network>> configured_networks() {
    return fail(errc::not_supported);
}

} // namespace wifi_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_WIFI_GENERIC_HPP
