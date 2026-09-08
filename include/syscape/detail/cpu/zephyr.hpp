#ifndef SYSCAPE_DETAIL_CPU_ZEPHYR_HPP
#define SYSCAPE_DETAIL_CPU_ZEPHYR_HPP

#include <syscape/detail/config.hpp>

#include <cerrno>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>

#if defined(SYSCAPE_ZEPHYR_HAS_POSIX_SINGLE_PROCESS)
#include <unistd.h>
#endif

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
#if defined(SYSCAPE_ZEPHYR_HAS_POSIX_SINGLE_PROCESS) &&                        \
    defined(_SC_NPROCESSORS_ONLN)
    errno = 0;
    const long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count > 0) {
        if (static_cast<unsigned long>(count) >
            (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        return static_cast<std::uint32_t>(count);
    }
    const int err = errno;
    if (count == 0) {
        return fail(errc::malformed_data);
    }
    if (err != 0 && err != EINVAL) {
        return fail(std::error_code(err, std::generic_category()));
    }
#endif
    return fail(errc::not_supported);
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
