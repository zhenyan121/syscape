#ifndef SYSCAPE_DETAIL_CPU_WINDOWS_HPP
#define SYSCAPE_DETAIL_CPU_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/cpu.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif

#if defined(WINVER) && WINVER < 0x0601
#error "syscape/cpu.hpp requires WINVER >= 0x0601 on Windows"
#endif

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#if !defined(WINVER)
#define SYSCAPE_DETAIL_CPU_DEFINED_WINVER
#define WINVER 0x0601
#endif

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <vector>
#include <windows.h>
#include <powerbase.h>

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
    const DWORD value = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (value == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> parse_relationship_count(
    const unsigned char* buffer, DWORD size,
    LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
    std::uint32_t count = 0U;
    std::size_t offset = 0U;
    while (offset < size) {
        const std::size_t remaining =
            static_cast<std::size_t>(size) - offset;
        struct relationship_header {
            LOGICAL_PROCESSOR_RELATIONSHIP relationship;
            DWORD size;
        };
        if (remaining < sizeof(relationship_header)) {
            return fail(errc::malformed_data);
        }
        relationship_header current {};
        std::memcpy(&current, buffer + offset, sizeof(current));
        if (current.size < sizeof(relationship_header) ||
            current.size > remaining || current.relationship != relationship) {
            return fail(errc::malformed_data);
        }
        if (count == (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        ++count;
        offset += current.size;
    }
    return count == 0U ? result<std::uint32_t>(fail(errc::malformed_data))
                       : result<std::uint32_t>(count);
}

inline result<std::uint32_t> relationship_count(
    LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
    constexpr DWORD maximum_size = 64U * 1024U * 1024U;
    DWORD size = 0U;
    if (::GetLogicalProcessorInformationEx(relationship, nullptr, &size) != FALSE) {
        return fail(errc::malformed_data);
    }
    const DWORD first_error = ::GetLastError();
    if (first_error != ERROR_INSUFFICIENT_BUFFER) {
        return fail(std::error_code(static_cast<int>(first_error),
                                    std::system_category()));
    }

    for (unsigned int attempt = 0U; attempt < 4U; ++attempt) {
        if (size == 0U || size > maximum_size) {
            return fail(size == 0U ? errc::malformed_data
                                  : errc::resource_exhausted);
        }
        std::unique_ptr<unsigned char[]> buffer(
            new (std::nothrow) unsigned char[size]);
        if (!buffer) { return fail(errc::resource_exhausted); }

        DWORD returned_size = size;
        auto* information = reinterpret_cast<
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.get());
        if (::GetLogicalProcessorInformationEx(
                relationship, information, &returned_size) != FALSE) {
            if (returned_size == 0U || returned_size > size) {
                return fail(errc::malformed_data);
            }
            return parse_relationship_count(
                buffer.get(), returned_size, relationship);
        }

        const DWORD error = ::GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(static_cast<int>(error),
                                        std::system_category()));
        }
        if (returned_size <= size) { return fail(errc::malformed_data); }
        size = returned_size;
    }
    return fail(errc::temporarily_unavailable);
}

inline result<std::uint32_t> online_physical_core_count() {
    return relationship_count(RelationProcessorCore);
}

inline result<std::uint32_t> online_processor_package_count() {
    return relationship_count(RelationProcessorPackage);
}

/// Requires a single active processor group for legacy group-relative APIs.
///
/// A zero group count is the documented failure sentinel. More than one group
/// means that neither GetSystemTimes nor the documented sizing procedure for
/// CallNtPowerInformation can satisfy a system-wide contract.
inline bool group_count_covers_system(std::size_t active_group_count) noexcept {
    return active_group_count == 1U;
}

inline result<void> require_single_active_processor_group() {
    const WORD active_group_count = ::GetActiveProcessorGroupCount();
    if (active_group_count == 0U) { return fail(errc::io_error); }
    return group_count_covers_system(active_group_count)
               ? result<void>()
               : result<void>(fail(errc::not_supported));
}

/// Converts the documented PROCESSOR_POWER_INFORMATION records into current
/// per-processor frequencies in kilohertz.
///
/// The API reports whole megahertz values, so each value multiplies by one
/// thousand exactly. A zero frequency cannot describe an operating
/// processor and is malformed platform data.
inline result<std::vector<std::uint32_t>> parse_current_frequencies(
    const ::PROCESSOR_POWER_INFORMATION* information, std::size_t count) {
    if (!information || count == 0U) { return fail(errc::malformed_data); }
    std::vector<std::uint32_t> values;
    values.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const std::uint64_t megahertz =
            static_cast<std::uint64_t>(information[index].CurrentMhz);
        if (megahertz == 0U) { return fail(errc::malformed_data); }
        const std::uint64_t kilohertz = megahertz * 1000U;
        if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        values.push_back(static_cast<std::uint32_t>(kilohertz));
    }
    return values;
}

/// Extracts the highest recorded processor clock in kilohertz from the
/// documented PROCESSOR_POWER_INFORMATION records.
inline result<std::uint32_t> parse_maximum_frequency(
    const ::PROCESSOR_POWER_INFORMATION* information, std::size_t count) {
    if (!information || count == 0U) { return fail(errc::malformed_data); }
    std::uint64_t maximum_megahertz = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        maximum_megahertz =
            maximum_megahertz < information[index].MaxMhz
                ? static_cast<std::uint64_t>(information[index].MaxMhz)
                : maximum_megahertz;
    }
    if (maximum_megahertz == 0U) { return fail(errc::malformed_data); }
    const std::uint64_t kilohertz = maximum_megahertz * 1000U;
    if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(kilohertz);
}

/// Reads one buffer of processor power records through the documented power
/// information interface.
///
/// The NTSTATUS result carries no standard-library error category, so a
/// failing status maps to io_error for the same reason Mach failures do on
/// other backends.
inline std::error_code processor_power_error(::NTSTATUS status) noexcept {
    // STATUS_ACCESS_DENIED is the documented NTSTATUS value 0xC0000022.
    // Converting the signed status to ULONG preserves its 32-bit value using
    // the standard unsigned conversion rules without requiring ntstatus.h.
    constexpr ::ULONG access_denied_status = 0xC0000022UL;
    return static_cast<::ULONG>(status) == access_denied_status
               ? make_error_code(errc::permission_denied)
               : make_error_code(errc::io_error);
}

inline result<std::size_t> query_processor_power_information(
    unsigned char* buffer, std::size_t byte_count) {
    const ::NTSTATUS status = ::CallNtPowerInformation(
        ProcessorInformation, nullptr, 0U, buffer,
        static_cast<::ULONG>(byte_count));
    if (status != 0) { return fail(processor_power_error(status)); }
    return byte_count / sizeof(::PROCESSOR_POWER_INFORMATION);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    const DWORD processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (processors == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    const std::size_t byte_count =
        static_cast<std::size_t>(processors) *
        sizeof(::PROCESSOR_POWER_INFORMATION);
    std::unique_ptr<unsigned char[]> buffer(
        new (std::nothrow) unsigned char[byte_count]);
    if (!buffer) { return fail(errc::resource_exhausted); }

    const result<std::size_t> returned =
        query_processor_power_information(buffer.get(), byte_count);
    if (!returned) { return fail(returned.error()); }
    if (*returned != processors) { return fail(errc::malformed_data); }

    return parse_current_frequencies(
        reinterpret_cast<const ::PROCESSOR_POWER_INFORMATION*>(buffer.get()),
        *returned);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    // The processor power information records expose no minimum operating
    // frequency, and Windows documents no other public source for that
    // contract.
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    const DWORD processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (processors == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    const std::size_t byte_count =
        static_cast<std::size_t>(processors) *
        sizeof(::PROCESSOR_POWER_INFORMATION);
    std::unique_ptr<unsigned char[]> buffer(
        new (std::nothrow) unsigned char[byte_count]);
    if (!buffer) { return fail(errc::resource_exhausted); }

    const result<std::size_t> returned =
        query_processor_power_information(buffer.get(), byte_count);
    if (!returned) { return fail(returned.error()); }
    if (*returned != processors) { return fail(errc::malformed_data); }

    return parse_maximum_frequency(
        reinterpret_cast<const ::PROCESSOR_POWER_INFORMATION*>(buffer.get()),
        *returned);
}

/// Folds the GetSystemTimes idle, kernel, and user totals into the portable
/// usage buckets.
///
/// The kernel total includes idle time by documentation, so system time is
/// the kernel total minus idle; a kernel total below idle contradicts the
/// documented invariant and is malformed platform data. All three inputs
/// are cumulative hundred-nanosecond counts.
inline result<cpu_common::usage_information> convert_system_times(
    std::uint64_t idle_ticks, std::uint64_t kernel_ticks,
    std::uint64_t user_ticks) {
    if (kernel_ticks < idle_ticks) { return fail(errc::malformed_data); }
    cpu_common::usage_information usage;
    usage.user_ticks = user_ticks;
    usage.system_ticks = kernel_ticks - idle_ticks;
    usage.idle_ticks = idle_ticks;
    return usage;
}

inline std::uint64_t filetime_to_uint64(const ::FILETIME& value) noexcept {
    ::ULARGE_INTEGER converted {};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    ::FILETIME idle_time {};
    ::FILETIME kernel_time {};
    ::FILETIME user_time {};
    if (::GetSystemTimes(&idle_time, &kernel_time, &user_time) == FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    return convert_system_times(filetime_to_uint64(idle_time),
                                filetime_to_uint64(kernel_time),
                                filetime_to_uint64(user_time));
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_DETAIL_CPU_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_CPU_DEFINED_WINVER
#endif

#if defined(SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT
#endif

#endif
