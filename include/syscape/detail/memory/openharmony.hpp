#ifndef SYSCAPE_DETAIL_MEMORY_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_MEMORY_OPENHARMONY_HPP

#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>

#include <syscape/detail/memory/common.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace memory_backend {

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno != 0
                   ? result<std::uint64_t>(
                         fail(std::error_code(errno, std::generic_category())))
                   : result<std::uint64_t>(fail(errc::malformed_data));
    }
    return static_cast<std::uint64_t>(value);
}

inline bool memory_space(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline std::string_view trim_memory_field(std::string_view value) noexcept {
    while (!value.empty() && memory_space(value.front())) {
        value.remove_prefix(1U);
    }
    while (!value.empty() && memory_space(value.back())) {
        value.remove_suffix(1U);
    }
    return value;
}

inline result<std::uint64_t> parse_kilobyte_amount(std::string_view input) {
    const std::string_view value = trim_memory_field(input);
    if (value.empty()) {
        return fail(errc::malformed_data);
    }
    std::uint64_t kilobytes = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed =
        std::from_chars(first, last, kilobytes);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr == first) {
        return fail(errc::malformed_data);
    }
    const std::string_view unit = trim_memory_field(
        value.substr(static_cast<std::size_t>(parsed.ptr - first)));
    if (unit != "kB") {
        return fail(errc::malformed_data);
    }
    constexpr std::uint64_t maximum_kilobytes =
        (std::numeric_limits<std::uint64_t>::max)() >> 10U;
    if (kilobytes > maximum_kilobytes) {
        return fail(errc::value_too_large);
    }
    return kilobytes << 10U;
}

inline result<std::uint64_t> extract_meminfo_field(std::string_view text,
                                                   std::string_view key) {
    std::size_t offset = 0U;
    while (offset < text.size()) {
        const std::size_t newline = text.find('\n', offset);
        const std::string_view line =
            (newline == std::string_view::npos)
                ? text.substr(offset)
                : text.substr(offset, newline - offset);
        offset =
            (newline == std::string_view::npos) ? text.size() : newline + 1U;

        if (line.size() > key.size() &&
            line.compare(0U, key.size(), key) == 0 && line[key.size()] == ':') {
            return parse_kilobyte_amount(line.substr(key.size() + 1U));
        }
    }
    return fail(errc::not_found);
}

inline result<std::uint64_t> physical_memory_bytes() {
    const auto content = openharmony::read_text_file("/proc/meminfo");
    if (!content) {
        return fail(content.error());
    }
    return extract_meminfo_field(*content, "MemTotal");
}

inline result<std::uint64_t> available_memory_bytes() {
    const auto content = openharmony::read_text_file("/proc/meminfo");
    if (!content) {
        return fail(content.error());
    }
    const auto avail = extract_meminfo_field(*content, "MemAvailable");
    if (avail) {
        return avail;
    }
    if (avail.error() != errc::not_found) {
        return fail(avail.error());
    }

    const auto free_m = extract_meminfo_field(*content, "MemFree");
    if (!free_m) {
        return fail(free_m.error());
    }
    const auto buffers = extract_meminfo_field(*content, "Buffers");
    if (!buffers && buffers.error() != errc::not_found) {
        return fail(buffers.error());
    }
    const auto cached = extract_meminfo_field(*content, "Cached");
    if (!cached && cached.error() != errc::not_found) {
        return fail(cached.error());
    }
    std::uint64_t extra = 0U;
    constexpr std::uint64_t max_u64 =
        (std::numeric_limits<std::uint64_t>::max)();
    if (buffers) {
        extra += *buffers;
    }
    if (cached) {
        if (*cached > max_u64 - extra) {
            return fail(errc::value_too_large);
        }
        extra += *cached;
    }
    if (*free_m > max_u64 - extra) {
        return fail(errc::value_too_large);
    }
    return *free_m + extra;
}

inline result<memory_common::swap_usage> swap_status() {
    const auto content = openharmony::read_text_file("/proc/meminfo");
    if (!content) {
        return fail(content.error());
    }
    const auto total = extract_meminfo_field(*content, "SwapTotal");
    if (!total) {
        return fail(total.error());
    }
    const auto free_bytes = extract_meminfo_field(*content, "SwapFree");
    if (!free_bytes) {
        return fail(free_bytes.error());
    }
    memory_common::swap_usage usage {};
    usage.total_bytes = *total;
    usage.free_bytes = *free_bytes;
    return usage;
}

inline result<memory_common::commit_usage> commit_status() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> huge_page_size_bytes() {
    return fail(errc::not_supported);
}

inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> memory_load_percent() {
    const result<std::uint64_t> physical = physical_memory_bytes();
    if (!physical) {
        return fail(physical.error());
    }
    const result<std::uint64_t> available = available_memory_bytes();
    if (!available) {
        return fail(available.error());
    }
    if (*available > *physical) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(*physical - *available,
                                              *physical);
}

inline result<memory_common::pressure_status> memory_pressure() {
    return fail(errc::not_supported);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
