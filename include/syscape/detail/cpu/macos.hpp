#ifndef SYSCAPE_DETAIL_CPU_MACOS_HPP
#define SYSCAPE_DETAIL_CPU_MACOS_HPP

#include <cerrno>
#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <sys/sysctl.h>
#include <vector>

#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

/// Owns one Mach host send right for the duration of a single query.
class host_port {
public:
    host_port() noexcept : value_(::mach_host_self()) {}
    host_port(const host_port&) = delete;
    host_port& operator=(const host_port&) = delete;
    ~host_port() {
        if (value_ != MACH_PORT_NULL) {
            ::mach_port_deallocate(::mach_task_self(), value_);
        }
    }

    /// Returns the owned port, or MACH_PORT_NULL after a failed acquisition.
    ::mach_port_t get() const noexcept { return value_; }

private:
    ::mach_port_t value_;
};

/// Releases the buffer that host_processor_info allocates implicitly.
class processor_info_buffer {
public:
    processor_info_buffer(processor_info_array_t information,
                          mach_msg_type_number_t count) noexcept
        : information_(information),
          count_(count) {}
    processor_info_buffer(const processor_info_buffer&) = delete;
    processor_info_buffer& operator=(const processor_info_buffer&) = delete;
    ~processor_info_buffer() {
        if (information_ != nullptr && count_ != 0U) {
            static_cast<void>(::vm_deallocate(
                ::mach_task_self(),
                reinterpret_cast<::vm_address_t>(information_),
                static_cast<::vm_size_t>(count_) * sizeof(integer_t)));
        }
    }

    /// Returns the owned per-processor tick records.
    processor_info_array_t get() const noexcept { return information_; }
    /// Returns the owned record length in integer_t elements.
    mach_msg_type_number_t size() const noexcept { return count_; }

private:
    processor_info_array_t information_;
    mach_msg_type_number_t count_;
};

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

/// Reads one documented 64-bit frequency sysctl and converts hertz to
/// kilohertz.
///
/// The conversion truncates sub-kilohertz remainders; recorded clock values
/// below one kilohertz cannot be represented by this contract and are
/// malformed platform data. A missing key means this platform exposes no
/// such frequency fact and reports not_supported.
inline result<std::uint32_t> frequency_sysctl_khz(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<std::uint32_t>(fail(errc::not_supported))
                   : result<std::uint32_t>(fail(std::error_code(
                         errno, std::generic_category())));
    }
    if (size != sizeof(std::uint64_t)) { return fail(errc::malformed_data); }
    std::uint64_t hertz = 0U;
    if (::sysctlbyname(name, &hertz, &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const std::uint64_t kilohertz = hertz / 1000U;
    if (kilohertz == 0U) { return fail(errc::malformed_data); }
    if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(kilohertz);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    return frequency_sysctl_khz("hw.cpufrequency_min");
}

inline result<std::uint32_t> maximum_frequency_khz() {
    return frequency_sysctl_khz("hw.cpufrequency_max");
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    // Darwin documents no public source for instantaneous per-processor or
    // system-wide current clocks.
    return fail(errc::not_supported);
}

/// Sums per-processor scheduler tick records into the portable usage
/// buckets.
///
/// Each processor contributes CPU_STATE_MAX consecutive natural_t counters.
/// host_processor_info exposes the allocated storage through its generic
/// integer_t array type, so each counter is copied into natural_t to preserve
/// its documented unsigned representation without violating aliasing rules.
/// User and nice states both describe user execution and fold into the user
/// bucket; other recorded states are outside the portable contract.
inline result<cpu_common::usage_information> sum_processor_load(
    const integer_t* records, std::size_t processor_count) {
    if (!records || processor_count == 0U) {
        return fail(errc::malformed_data);
    }
    cpu_common::usage_information usage;
    for (std::size_t processor = 0U; processor < processor_count;
         ++processor) {
        const integer_t* ticks =
            records + static_cast<std::size_t>(processor) *
                          static_cast<std::size_t>(CPU_STATE_MAX);
        const std::size_t indices[4] = {CPU_STATE_USER, CPU_STATE_NICE,
                                        CPU_STATE_SYSTEM, CPU_STATE_IDLE};
        natural_t states[4] = {};
        for (std::size_t state = 0U; state < 4U; ++state) {
            static_assert(sizeof(natural_t) == sizeof(integer_t),
                          "Mach processor tick types must have equal size");
            std::memcpy(&states[state], ticks + indices[state],
                        sizeof(states[state]));
        }
        const std::uint64_t user_ticks =
            static_cast<std::uint64_t>(states[0]) + states[1];
        const std::uint64_t system_ticks = states[2];
        const std::uint64_t idle_ticks = states[3];
        usage.user_ticks += user_ticks;
        usage.system_ticks += system_ticks;
        usage.idle_ticks += idle_ticks;
        if (usage.user_ticks < user_ticks ||
            usage.system_ticks < system_ticks ||
            usage.idle_ticks < idle_ticks) {
            return fail(errc::value_too_large);
        }
    }
    return usage;
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    const host_port host;
    if (host.get() == MACH_PORT_NULL) { return fail(errc::io_error); }

    natural_t processor_count = 0U;
    processor_info_array_t information = nullptr;
    mach_msg_type_number_t information_count = 0U;
    const kern_return_t status = ::host_processor_info(
        host.get(), PROCESSOR_CPU_LOAD_INFO, &processor_count,
        &information, &information_count);
    if (status != KERN_SUCCESS) { return fail(errc::io_error); }
    const processor_info_buffer owned(information, information_count);

    if (processor_count == 0U ||
        static_cast<std::size_t>(information_count) !=
            static_cast<std::size_t>(processor_count) *
                static_cast<std::size_t>(CPU_STATE_MAX)) {
        return fail(errc::malformed_data);
    }
    return sum_processor_load(owned.get(), processor_count);
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
