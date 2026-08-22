#ifndef SYSCAPE_DETAIL_MEMORY_LINUX_HPP
#define SYSCAPE_DETAIL_MEMORY_LINUX_HPP

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/memory/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

/// Parsed /proc/meminfo capacity fields, stored in bytes.
struct memory_information {
    std::uint64_t total_bytes = 0U;
    bool has_total = false;
    std::uint64_t available_bytes = 0U;
    bool has_available = false;
    std::uint64_t swap_total_bytes = 0U;
    bool has_swap_total = false;
    std::uint64_t swap_free_bytes = 0U;
    bool has_swap_free = false;
};

inline bool memory_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline std::string_view trim_memory_field(std::string_view value) noexcept {
    while (!value.empty() && memory_space(value.front())) { value.remove_prefix(1U); }
    while (!value.empty() && memory_space(value.back())) { value.remove_suffix(1U); }
    return value;
}

/// Parses one /proc/meminfo amount such as "16384252 kB" into bytes.
///
/// The kernel documents every meminfo size with an explicit kibibyte suffix.
/// Only that documented "kB" form is accepted; missing or different units
/// are malformed platform data rather than assumed kilobytes.
inline result<std::uint64_t> parse_kilobyte_amount(std::string_view input) {
    const std::string_view value = trim_memory_field(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint64_t kilobytes = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed =
        std::from_chars(first, last, kilobytes);
    if (parsed.ec != std::errc() || parsed.ptr == first) {
        return fail(errc::malformed_data);
    }
    const std::string_view unit =
        trim_memory_field(value.substr(static_cast<std::size_t>(parsed.ptr - first)));
    if (unit != "kB") { return fail(errc::malformed_data); }
    constexpr std::uint64_t maximum_kilobytes =
        (std::numeric_limits<std::uint64_t>::max)() >> 10U;
    if (kilobytes > maximum_kilobytes) { return fail(errc::value_too_large); }
    return kilobytes << 10U;
}

/// Parses the documented fields of a /proc/meminfo snapshot.
///
/// Only the recognized keys are parsed; every unrecognized line is skipped
/// before its value is examined, so unknown fields with any format never
/// affect the result. A recognized key with an unparsable or empty amount
/// fails with malformed_data. Duplicate recognized keys keep the final
/// value, matching sequential reads of the file.
inline result<memory_information> parse_meminfo(std::string_view input) {
    memory_information output;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        const std::size_t separator = line.find(':');
        if (separator == std::string_view::npos) { continue; }
        const std::string_view key = trim_memory_field(line.substr(0U, separator));

        const bool is_total = key == "MemTotal";
        const bool is_available = !is_total && key == "MemAvailable";
        const bool is_swap_total =
            !is_total && !is_available && key == "SwapTotal";
        const bool is_swap_free = !is_total && !is_available &&
                                  !is_swap_total && key == "SwapFree";
        if (!is_total && !is_available && !is_swap_total && !is_swap_free) {
            continue;
        }

        const result<std::uint64_t> bytes =
            parse_kilobyte_amount(line.substr(separator + 1U));
        if (!bytes) { return fail(bytes.error()); }
        if (is_total) {
            output.total_bytes = *bytes;
            output.has_total = true;
        } else if (is_available) {
            output.available_bytes = *bytes;
            output.has_available = true;
        } else if (is_swap_total) {
            output.swap_total_bytes = *bytes;
            output.has_swap_total = true;
        } else {
            output.swap_free_bytes = *bytes;
            output.has_swap_free = true;
        }
    }
    return output;
}

inline result<memory_information> read_memory_information() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/meminfo");
    if (!content) { return fail(content.error()); }
    return parse_meminfo(*content);
}

inline result<std::uint64_t> page_size_bytes() {
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno == 0
                   ? result<std::uint64_t>(fail(errc::not_supported))
                   : result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    return static_cast<std::uint64_t>(value);
}

inline result<std::uint64_t> physical_memory_bytes() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    if (!information->has_total) { return fail(errc::not_found); }
    return information->total_bytes;
}

inline result<std::uint64_t> available_memory_bytes() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    // Kernels before MemAvailable's introduction do not expose this estimate.
    if (!information->has_available) { return fail(errc::not_supported); }
    return information->available_bytes;
}

inline result<memory_common::swap_usage> swap_status() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    if (!information->has_swap_total || !information->has_swap_free) {
        return fail(errc::not_found);
    }
    memory_common::swap_usage usage;
    usage.total_bytes = information->swap_total_bytes;
    usage.free_bytes = information->swap_free_bytes;
    return usage;
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
