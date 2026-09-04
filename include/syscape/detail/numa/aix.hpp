#ifndef SYSCAPE_DETAIL_NUMA_AIX_HPP
#define SYSCAPE_DETAIL_NUMA_AIX_HPP

#include <cstdint>
#include <vector>

#include <syscape/detail/numa/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace numa_backend {

inline result<bool> is_numa_available() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> node_count() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::numa::numa_node>> nodes() {
    return fail(errc::not_supported);
}

inline result<::syscape::numa::numa_node> node(std::uint32_t) {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> current_thread_node() {
    return fail(errc::not_supported);
}

} // namespace numa_backend
} // namespace detail
} // namespace syscape

#endif
