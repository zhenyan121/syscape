#ifndef SYSCAPE_DETAIL_CPU_MACOS_HPP
#define SYSCAPE_DETAIL_CPU_MACOS_HPP

#include <algorithm>
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

/// Darwin exposes per-level cache geometry but no documented mapping from
/// those values to distinct sharing sets. The portable contract requires one
/// entry per cache instance, so returning one aggregate entry per level would
/// undercount multicore systems and is not an honest implementation.
inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    return fail(errc::not_supported);
}

/// Reads one documented string sysctl value.
///
/// A missing key means this platform exposes no such recording and reports
/// not_supported; trailing NUL padding is removed so the text can be split
/// into tokens safely.
inline result<std::string> string_sysctl(const char* name) {
    std::size_t size = 0U;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<std::string>(fail(errc::not_supported))
                   : result<std::string>(fail(std::error_code(
                         errno, std::generic_category())));
    }
    if (size == 0U) { return fail(errc::malformed_data); }
    std::string value(size, '\0');
    if (::sysctlbyname(name, &value[0], &size, nullptr, 0U) != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    value.resize(size);
    while (!value.empty() && value.back() == '\0') { value.pop_back(); }
    return value;
}

/// Reads one documented boolean hw.optional probe.
///
/// A missing key means this platform records no such concept and reports
/// not_supported; a response of another width is malformed platform data.
inline result<int> optional_flag_sysctl(const char* name) {
    int value = 0;
    std::size_t size = sizeof(value);
    if (::sysctlbyname(name, &value, &size, nullptr, 0U) != 0) {
        return errno == ENOENT || errno == EOPNOTSUPP
                   ? result<int>(fail(errc::not_supported))
                   : result<int>(fail(std::error_code(
                         errno, std::generic_category())));
    }
    if (size != sizeof(value)) { return fail(errc::malformed_data); }
    return value;
}

/// Interprets the documented extensible hw.optional value convention.
///
/// Apple requires callers to test for nonzero because future selectors may
/// return values greater than one.
inline bool optional_flag_is_present(int value) noexcept {
    return value != 0;
}

/// Appends every whitespace-separated token of one rendering, preserving
/// first-seen order and skipping repeats.
inline void append_whitespace_tokens(std::vector<std::string>& tokens,
                                     const std::string& value) {
    std::size_t offset = 0U;
    while (offset < value.size()) {
        const unsigned char current =
            static_cast<unsigned char>(value[offset]);
        if (current == ' ' || current == '\t' || current == '\r' ||
            current == '\n') {
            ++offset;
            continue;
        }
        const std::size_t start = offset;
        while (offset < value.size()) {
            const unsigned char following =
                static_cast<unsigned char>(value[offset]);
            if (following == ' ' || following == '\t' ||
                following == '\r' || following == '\n') {
                break;
            }
            ++offset;
        }
        const std::string token = value.substr(start, offset - start);
        if (std::find(tokens.begin(), tokens.end(), token) ==
            tokens.end()) {
            tokens.push_back(token);
        }
        ++offset;
    }
}

/// One documented boolean capability probe with its rendered identifier.
struct optional_flag_probe {
    /// The documented sysctl key without its hw.optional. prefix.
    const char* suffix;
};

/// The documented hw.optional keys this slice probes.
///
/// The table spans both Intel-era and Apple silicon renderings because the
/// same interface serves both processor families; absent keys are skipped,
/// so a platform simply reports the vocabulary it records.
inline constexpr optional_flag_probe optional_flag_probes[] = {
    {"neon"},         {"neon_fp16"},
    {"neon_hpfp"},    {"neon_spfp"},
    {"armv8_crc32"},  {"armv8_pmull_v8"},
    {"armv8_sha1"},   {"armv8_sha2"},
    {"armv8_1_atomics"},
    {"armv8_2_fhm"},
    {"armv8_2_sha512"},
    {"armv8_2_sha3"},
    {"mmx"},          {"sse"},
    {"sse2"},         {"sse3"},
    {"supplementalsse3"},
    {"sse4_1"},       {"sse4_2"},
    {"avx1_0"},       {"avx2_0"},
    {"f16c"},         {"fma3"},
    {"aes"},          {"sha256"},
    {"sha512"},       {"bmi1"},
    {"bmi2"},         {"adx"},
    {"rdrand"},       {"rdseed"},
};

inline result<std::vector<std::string>> instruction_set_features() {
    std::vector<std::string> features;
    bool saw_any_source = false;

    static constexpr const char* machdep_keys[] = {
        "machdep.cpu.features", "machdep.cpu.leaf7_features"};
    for (const char* name : machdep_keys) {
        const result<std::string> text = string_sysctl(name);
        if (text) {
            saw_any_source = true;
            append_whitespace_tokens(features, *text);
        } else if (text.error() != errc::not_supported) {
            return fail(text.error());
        }
    }

    for (const optional_flag_probe& probe : optional_flag_probes) {
        const std::string key =
            std::string("hw.optional.") + probe.suffix;
        const result<int> flag = optional_flag_sysctl(key.c_str());
        if (flag) {
            saw_any_source = true;
            if (optional_flag_is_present(*flag) &&
                std::find(features.begin(), features.end(),
                          probe.suffix) == features.end()) {
                features.emplace_back(probe.suffix);
            }
        } else if (flag.error() != errc::not_supported) {
            return fail(flag.error());
        }
    }

    if (!saw_any_source) {
        return result<std::vector<std::string>>(fail(errc::not_supported));
    }
    return features;
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
