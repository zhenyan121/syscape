#ifndef SYSCAPE_DETAIL_INPUT_GENERIC_HPP
#define SYSCAPE_DETAIL_INPUT_GENERIC_HPP

#include <cstddef>
#include <vector>

#include <syscape/input.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace input_backend {

inline result<std::vector<::syscape::input::input_device>> devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::input::input_device>> keyboards() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::input::input_device>> mice() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::input::input_device>> touch_devices() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::input::input_device>> gamepads() {
    return fail(errc::not_supported);
}

inline result<std::size_t> device_count() {
    return fail(errc::not_supported);
}

} // namespace input_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_INPUT_GENERIC_HPP
