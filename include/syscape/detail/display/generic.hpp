#ifndef SYSCAPE_DETAIL_DISPLAY_GENERIC_HPP
#define SYSCAPE_DETAIL_DISPLAY_GENERIC_HPP

#include <cstddef>
#include <vector>

#include <syscape/display.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace display_backend {

inline result<std::vector<::syscape::display::display_info>> displays() {
    return fail(errc::not_supported);
}

inline result<std::size_t> display_count() {
    return fail(errc::not_supported);
}

inline result<::syscape::display::display_info> primary_display() {
    return fail(errc::not_supported);
}

} // namespace display_backend
} // namespace detail
} // namespace syscape

#endif
