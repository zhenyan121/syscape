#ifndef SYSCAPE_DETAIL_BLUETOOTH_GENERIC_HPP
#define SYSCAPE_DETAIL_BLUETOOTH_GENERIC_HPP

#include <cstddef>
#include <vector>

#include <syscape/bluetooth.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace bluetooth_backend {

inline result<std::vector<::syscape::bluetooth::adapter_info>> adapters() {
    return fail(errc::not_supported);
}

inline result<std::size_t> adapter_count() {
    return fail(errc::not_supported);
}

inline result<::syscape::bluetooth::adapter_info> default_adapter() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::bluetooth::device_info>> paired_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::bluetooth::device_info>> connected_devices() {
    return fail(errc::not_supported);
}

} // namespace bluetooth_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_BLUETOOTH_GENERIC_HPP
