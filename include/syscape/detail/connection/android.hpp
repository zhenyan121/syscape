#ifndef SYSCAPE_DETAIL_CONNECTION_ANDROID_HPP
#define SYSCAPE_DETAIL_CONNECTION_ANDROID_HPP

#include <vector>

#include <syscape/detail/connection/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace connection_backend {

inline result<std::vector<connection_common::connection_record>> connections() {
    return fail(errc::not_supported);
}

} // namespace connection_backend
} // namespace detail
} // namespace syscape

#endif
