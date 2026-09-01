#ifndef SYSCAPE_DETAIL_PRINTER_ANDROID_HPP
#define SYSCAPE_DETAIL_PRINTER_ANDROID_HPP

#include <cstddef>
#include <string_view>
#include <vector>

#include <syscape/error.hpp>
#include <syscape/printer.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_backend {

inline result<std::vector<printer::printer_info>> printers() {
    return fail(errc::not_supported);
}

inline result<std::size_t> printer_count() {
    return fail(errc::not_supported);
}

inline result<printer::printer_info> default_printer() {
    return fail(errc::not_supported);
}

inline result<printer::printer_info> find_printer(std::string_view name) {
    static_cast<void>(name);
    return fail(errc::not_supported);
}

} // namespace printer_backend
} // namespace detail
} // namespace syscape

#endif
