#ifndef SYSCAPE_DETAIL_CPU_MACOS_HPP
#define SYSCAPE_DETAIL_CPU_MACOS_HPP

#include <cerrno>
#include <cstdint>
#include <string>
#include <system_error>
#include <sys/sysctl.h>
#include <vector>

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

inline result<std::uint32_t> positive_sysctl_count(const char* name) {
    std::uint32_t value = 0U;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (size != sizeof(value) || value == 0U) {
        return fail(errc::malformed_data);
    }
    return value;
}

inline result<std::uint32_t> online_logical_processor_count() {
    return positive_sysctl_count("hw.logicalcpu");
}

inline result<std::uint32_t> online_physical_core_count() {
    return positive_sysctl_count("hw.physicalcpu");
}

inline result<std::uint32_t> online_processor_package_count() {
    return fail(errc::not_supported);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
