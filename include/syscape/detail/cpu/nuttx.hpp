#ifndef SYSCAPE_DETAIL_CPU_NUTTX_HPP
#define SYSCAPE_DETAIL_CPU_NUTTX_HPP

#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}
inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_logical_processor_count() {
    errno = 0;
    const long count = ::sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) {
        const auto value = static_cast<unsigned long>(count);
        if (value > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint32_t>(value);
    }
    if (count == 0) {
        return fail(errc::malformed_data);
    }
    const int error = errno;
    if (error == EINVAL || error == ENOSYS || error == 0) {
        return fail(errc::not_supported);
    }
    return fail(std::error_code(error, std::generic_category()));
}

inline result<std::uint32_t> online_physical_core_count() {
    return fail(errc::not_supported);
}
inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}
inline result<std::uint32_t> minimum_frequency_khz() {
    return fail(errc::not_supported);
}
inline result<std::uint32_t> maximum_frequency_khz() {
    return fail(errc::not_supported);
}
inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    return fail(errc::not_supported);
}
inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}
inline result<std::vector<std::string>> instruction_set_features() {
    return fail(errc::not_supported);
}
inline result<cpu_common::usage_information> cumulative_processor_usage() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
