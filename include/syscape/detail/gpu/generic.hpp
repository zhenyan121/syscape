#ifndef SYSCAPE_DETAIL_GPU_GENERIC_HPP
#define SYSCAPE_DETAIL_GPU_GENERIC_HPP

#include <cstddef>
#include <vector>

#include <syscape/gpu.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace gpu_backend {

inline result<std::vector<::syscape::gpu::gpu_device>> devices() {
    return fail(errc::not_supported);
}

inline result<std::size_t> device_count() {
    return fail(errc::not_supported);
}

inline result<::syscape::gpu::gpu_device> primary_device() {
    return fail(errc::not_supported);
}

} // namespace gpu_backend
} // namespace detail
} // namespace syscape

#endif
