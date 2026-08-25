#ifndef SYSCAPE_DETAIL_PROCESS_LINUX_HPP
#define SYSCAPE_DETAIL_PROCESS_LINUX_HPP

#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <sched.h>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/linux/process_metrics.hpp>
#include <syscape/detail/os/linux.hpp>
#include <syscape/detail/process/common.hpp>
#include <syscape/detail/process/posix.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_backend {

using linux_process_metrics::compose_start_time;
using linux_process_metrics::scale_resident_bytes;
using linux_process_metrics::ticks_to_nanoseconds;

inline result<std::uint32_t> process_id_value(
    pid_t value, bool zero_is_valid) {
    if (value < 0 || (!zero_is_valid && value == 0)) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::uint32_t>(value);
}

inline result<std::uint32_t> process_id() {
    return process_id_value(::getpid(), false);
}

inline result<std::uint32_t> parent_process_id() {
    return process_id_value(::getppid(), true);
}

inline result<std::vector<std::string>> parse_command_line(
    std::string_view input) {
    if (input.empty()) { return std::vector<std::string>(); }
    if (input.back() != '\0') { return fail(errc::malformed_data); }
    input.remove_suffix(1U);

    std::vector<std::string> arguments;
    std::size_t start = 0U;
    for (;;) {
        const std::size_t end = input.find('\0', start);
        if (end == std::string_view::npos) {
            arguments.emplace_back(input.substr(start));
            break;
        }
        arguments.emplace_back(input.substr(start, end - start));
        start = end + 1U;
    }
    return arguments;
}

template <typename ReadLinkOperation>
inline result<std::string> read_link_with_growth(
    ReadLinkOperation read_link) {
    constexpr std::size_t initial_size = 256U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        errno = 0;
        const ssize_t count =
            read_link(buffer.data(), buffer.size());
        if (count >= 0 && static_cast<std::size_t>(count) < buffer.size()) {
            return std::string(buffer.data(), static_cast<std::size_t>(count));
        }
        if (count < 0) {
            if (errno != EINTR) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            continue;
        }
        if (buffer.size() > maximum_size / 2U) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() * 2U);
    }
}

inline result<std::string> executable_path() {
    const result<std::string> value = read_link_with_growth(
        [](char* buffer, std::size_t size) {
            return ::readlink("/proc/self/exe", buffer, size);
        });
    if (!value || value->empty() || value->front() != '/') {
        return value ? result<std::string>(fail(errc::malformed_data))
                     : result<std::string>(fail(value.error()));
    }
    return value;
}

inline result<std::vector<std::string>> command_line() {
    result<std::string> input =
        linux_platform::read_text_file("/proc/self/cmdline");
    if (!input) { return fail(input.error()); }
    return parse_command_line(*input);
}

inline result<std::string> working_directory() {
    std::vector<char> buffer(1024U);
    constexpr std::size_t maximum_size = 1024U * 1024U;
    for (;;) {
        errno = 0;
        const char* value = ::getcwd(buffer.data(), buffer.size());
        if (value != nullptr) {
            const std::string path(buffer.data());
            return path.empty() ? result<std::string>(fail(errc::malformed_data))
                                : result<std::string>(std::move(path));
        }
        if (errno != ERANGE) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (buffer.size() >= maximum_size) {
            return fail(errc::value_too_large);
        }
        buffer.resize(buffer.size() <= maximum_size / 2U
                          ? buffer.size() * 2U
                          : maximum_size);
    }
}

/// Parsed runtime-attribute fields of one /proc/self/stat snapshot.
///
/// Time fields keep their raw clock-tick counts until the caller converts
/// them with a validated clock-ticks-per-second value.
struct runtime_statistics {
    /// Field 14: user-mode time in clock ticks.
    std::uint64_t user_ticks = 0U;
    /// Field 15: kernel-mode time in clock ticks.
    std::uint64_t system_ticks = 0U;
    /// Field 20: number of live threads. Always at least one.
    std::uint32_t threads = 0U;
    /// Field 22: clock ticks after system boot when the process started.
    std::uint64_t start_ticks = 0U;
    /// Field 23: total virtual address-space size in bytes.
    std::uint64_t virtual_size_bytes = 0U;
    /// Field 24: resident-set size in pages of the running page size.
    std::uint64_t resident_pages = 0U;
};

inline bool stat_separator(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
           value == '\f' || value == '\v';
}

inline result<std::uint64_t> parse_stat_field(std::string_view token) {
    if (token.empty()) { return fail(errc::malformed_data); }
    std::uint64_t value = 0U;
    const char* first = token.data();
    const char* last = first + token.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Parses the documented fields of one /proc/self/stat snapshot.
///
/// The kernel wraps the process name in parentheses and the name itself may
/// contain spaces and parentheses, so the value boundary is the first '('
/// and the last ')'. Fields after the name are whitespace-separated; index
/// zero is field three (state) of the proc(5) table. At least the first 22
/// post-name fields must be present; later fields are ignored.
inline result<runtime_statistics> parse_stat(std::string_view input) {
    constexpr std::size_t required_fields = 22U;
    const std::size_t name_begin = input.find('(');
    const std::size_t name_end = input.rfind(')');
    if (name_begin == std::string_view::npos ||
        name_end == std::string_view::npos || name_end < name_begin) {
        return fail(errc::malformed_data);
    }
    const std::string_view remainder = input.substr(name_end + 1U);

    std::array<std::string_view, required_fields> fields {};
    std::size_t found = 0U;
    std::size_t position = 0U;
    while (position < remainder.size() && found < required_fields) {
        while (position < remainder.size() &&
               stat_separator(remainder[position])) {
            ++position;
        }
        if (position >= remainder.size()) { break; }
        const std::size_t start = position;
        while (position < remainder.size() &&
               !stat_separator(remainder[position])) {
            ++position;
        }
        fields[found] = remainder.substr(start, position - start);
        ++found;
    }
    if (found < required_fields) { return fail(errc::malformed_data); }

    runtime_statistics statistics;
    const result<std::uint64_t> user_ticks = parse_stat_field(fields[11]);
    if (!user_ticks) { return fail(user_ticks.error()); }
    const result<std::uint64_t> system_ticks = parse_stat_field(fields[12]);
    if (!system_ticks) { return fail(system_ticks.error()); }
    const result<std::uint64_t> threads = parse_stat_field(fields[17]);
    if (!threads) { return fail(threads.error()); }
    if (*threads == 0U) { return fail(errc::malformed_data); }
    if (*threads > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    const result<std::uint64_t> start_ticks = parse_stat_field(fields[19]);
    if (!start_ticks) { return fail(start_ticks.error()); }
    const result<std::uint64_t> virtual_size = parse_stat_field(fields[20]);
    if (!virtual_size) { return fail(virtual_size.error()); }
    const result<std::uint64_t> resident_pages = parse_stat_field(fields[21]);
    if (!resident_pages) { return fail(resident_pages.error()); }

    statistics.user_ticks = *user_ticks;
    statistics.system_ticks = *system_ticks;
    statistics.threads = static_cast<std::uint32_t>(*threads);
    statistics.start_ticks = *start_ticks;
    statistics.virtual_size_bytes = *virtual_size;
    statistics.resident_pages = *resident_pages;
    return statistics;
}

inline result<runtime_statistics> read_runtime_statistics() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/self/stat");
    if (!content) { return fail(content.error()); }
    return parse_stat(*content);
}

inline result<long> clock_ticks_per_second() {
    errno = 0;
    const long value = ::sysconf(_SC_CLK_TCK);
    if (value <= 0) {
        return errno == 0
                   ? result<long>(fail(errc::not_supported))
                   : result<long>(
                         fail(std::error_code(errno, std::generic_category())));
    }
    return value;
}

inline result<std::uint64_t> page_size_bytes() {
    errno = 0;
    const long value = ::sysconf(_SC_PAGESIZE);
    if (value <= 0) {
        return errno == 0
                   ? result<std::uint64_t>(fail(errc::not_supported))
                   : result<std::uint64_t>(fail(std::error_code(
                         errno, std::generic_category())));
    }
    return static_cast<std::uint64_t>(value);
}

inline result<process_common::cpu_time_usage> cpu_time() {
    const result<runtime_statistics> statistics = read_runtime_statistics();
    if (!statistics) { return fail(statistics.error()); }
    const result<long> rate = clock_ticks_per_second();
    if (!rate) { return fail(rate.error()); }
    const result<std::chrono::nanoseconds> user =
        ticks_to_nanoseconds(statistics->user_ticks, *rate);
    if (!user) { return fail(user.error()); }
    const result<std::chrono::nanoseconds> system =
        ticks_to_nanoseconds(statistics->system_ticks, *rate);
    if (!system) { return fail(system.error()); }
    process_common::cpu_time_usage usage;
    usage.user = *user;
    usage.system = *system;
    return usage;
}

inline result<std::chrono::system_clock::time_point> start_time() {
    const result<runtime_statistics> statistics = read_runtime_statistics();
    if (!statistics) { return fail(statistics.error()); }
    const result<long> rate = clock_ticks_per_second();
    if (!rate) { return fail(rate.error()); }
    const result<std::chrono::nanoseconds> age =
        ticks_to_nanoseconds(statistics->start_ticks, *rate);
    if (!age) { return fail(age.error()); }
    const result<std::chrono::system_clock::time_point> boot =
        os_backend::boot_time();
    if (!boot) { return fail(boot.error()); }
    return compose_start_time(*boot, *age);
}

inline result<std::uint32_t> thread_count() {
    const result<runtime_statistics> statistics = read_runtime_statistics();
    if (!statistics) { return fail(statistics.error()); }
    return statistics->threads;
}

inline result<process_common::memory_usage_snapshot> memory_usage() {
    const result<runtime_statistics> statistics = read_runtime_statistics();
    if (!statistics) { return fail(statistics.error()); }
    const result<std::uint64_t> page = page_size_bytes();
    if (!page) { return fail(page.error()); }
    const result<std::uint64_t> resident =
        scale_resident_bytes(statistics->resident_pages, *page);
    if (!resident) { return fail(resident.error()); }
    process_common::memory_usage_snapshot usage;
    usage.resident_bytes = *resident;
    usage.virtual_bytes = statistics->virtual_size_bytes;
    return usage;
}

/// Validates the documented Linux nice range.
///
/// The kernel documents nice values from -20 (most favorable) through 19
/// (least favorable); anything outside that range cannot come from the
/// documented source.
inline result<int> validate_priority(int value) {
    return process_posix::validate_priority(value, -20, 19);
}

inline result<int> priority() {
    return process_posix::priority(-20, 19);
}

/// Expands a kernel affinity bitmask into ascending logical processor
/// indices.
///
/// The mask layout follows the cpu_set_t representation: one bit per logical
/// processor index inside an array of native unsigned long words. An empty
/// mask cannot describe a runnable process and is malformed platform data.
inline result<std::vector<std::uint32_t>> affinity_indices(
    const unsigned long* words, std::size_t word_count) {
    std::vector<std::uint32_t> indices;
    for (std::size_t word = 0U; word < word_count; ++word) {
        for (std::size_t bit = 0U; bit < sizeof(unsigned long) * 8U; ++bit) {
            if (((words[word] >> bit) & 1UL) != 0UL) {
                const std::uint64_t index =
                    static_cast<std::uint64_t>(word) *
                        static_cast<std::uint64_t>(sizeof(unsigned long)) *
                        8ULL +
                    bit;
                if (index > (std::numeric_limits<std::uint32_t>::max)()) {
                    return fail(errc::value_too_large);
                }
                indices.push_back(static_cast<std::uint32_t>(index));
            }
        }
    }
    if (indices.empty()) { return fail(errc::malformed_data); }
    return indices;
}

inline result<std::vector<std::uint32_t>> cpu_affinity() {
    static_assert(sizeof(::cpu_set_t) % sizeof(unsigned long) == 0U,
                  "cpu_set_t must be an array of unsigned long words");
    constexpr std::size_t initial_words =
        sizeof(::cpu_set_t) / sizeof(unsigned long);
    // The kernel rejects masks smaller than the possible-CPU range with
    // EINVAL, so the buffer grows until the syscall accepts it. The growth
    // cap keeps a hostile kernel from forcing unbounded allocation.
    constexpr std::size_t maximum_words = initial_words * 512U;

    std::vector<unsigned long> words(initial_words, 0UL);
    for (;;) {
        const int status = ::sched_getaffinity(
            0, words.size() * sizeof(unsigned long),
            reinterpret_cast<::cpu_set_t*>(words.data()));
        if (status == 0) { break; }
        const int error = errno;
        if (error != EINVAL || words.size() >= maximum_words) {
            return error == EINVAL && words.size() >= maximum_words
                       ? result<std::vector<std::uint32_t>>(
                             fail(errc::value_too_large))
                       : result<std::vector<std::uint32_t>>(fail(
                             std::error_code(error,
                                             std::generic_category())));
        }
        words.resize(words.size() * 2U, 0UL);
    }
    return affinity_indices(words.data(), words.size());
}

inline result<process_common::resource_limit_snapshot> resource_limit(
    process_common::limit_resource kind) {
    return process_posix::resource_limit(kind);
}

} // namespace process_backend
} // namespace detail
} // namespace syscape

#endif
