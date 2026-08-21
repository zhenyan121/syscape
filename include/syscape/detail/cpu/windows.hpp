#ifndef SYSCAPE_DETAIL_CPU_WINDOWS_HPP
#define SYSCAPE_DETAIL_CPU_WINDOWS_HPP

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

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#endif
