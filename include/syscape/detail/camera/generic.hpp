#ifndef SYSCAPE_DETAIL_CAMERA_GENERIC_HPP
#define SYSCAPE_DETAIL_CAMERA_GENERIC_HPP

#include <cstddef>
#include <vector>

#include <syscape/camera.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace camera_backend {

inline result<std::vector<::syscape::camera::camera_device>> devices() {
    return fail(errc::not_supported);
}

inline result<std::size_t> device_count() { return fail(errc::not_supported); }

inline result<std::vector<::syscape::camera::camera_device>> capture_devices() {
    return fail(errc::not_supported);
}

inline result<::syscape::camera::camera_device> default_device() {
    return fail(errc::not_supported);
}

} // namespace camera_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_CAMERA_GENERIC_HPP
