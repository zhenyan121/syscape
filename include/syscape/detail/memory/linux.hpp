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
    std::uint64_t committed_bytes = 0U;
    bool has_committed = false;
    std::uint64_t commit_limit_bytes = 0U;
    bool has_commit_limit = false;
    std::uint64_t huge_total_count = 0U;
    bool has_huge_total = false;
    std::uint64_t huge_free_count = 0U;
    bool has_huge_free = false;
    std::uint64_t huge_page_size_bytes = 0U;
    bool has_huge_page_size = false;
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

/// Parses one unitless /proc/meminfo count such as "16".
///
/// The kernel documents the HugePages_ records as plain page counts without
/// any unit suffix. A missing number or trailing text is malformed platform
/// data rather than a truncated read.
inline result<std::uint64_t> parse_bare_count(std::string_view input) {
    const std::string_view value = trim_memory_field(input);
    if (value.empty()) { return fail(errc::malformed_data); }
    std::uint64_t count = 0U;
    const char* first = value.data();
    const char* last = first + value.size();
    const std::from_chars_result parsed = std::from_chars(first, last, count);
    if (parsed.ec != std::errc() || parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return count;
}

/// Recognized /proc/meminfo fields of this backend.
enum class meminfo_field {
    unrecognized,
    total,
    available,
    swap_total,
    swap_free,
    committed,
    commit_limit,
    huge_total_count,
    huge_free_count,
    huge_page_size
};

/// One documented key-to-field mapping of the meminfo table.
struct meminfo_key_mapping {
    std::string_view name;
    meminfo_field field;
};

inline constexpr meminfo_key_mapping documented_meminfo_keys[] = {
    {"MemTotal", meminfo_field::total},
    {"MemAvailable", meminfo_field::available},
    {"SwapTotal", meminfo_field::swap_total},
    {"SwapFree", meminfo_field::swap_free},
    {"Committed_AS", meminfo_field::committed},
    {"CommitLimit", meminfo_field::commit_limit},
    {"HugePages_Total", meminfo_field::huge_total_count},
    {"HugePages_Free", meminfo_field::huge_free_count},
    {"Hugepagesize", meminfo_field::huge_page_size}};

/// Classifies one meminfo key against the documented mapping table.
inline meminfo_field classify_meminfo_key(std::string_view key) {
    for (const meminfo_key_mapping& mapping : documented_meminfo_keys) {
        if (mapping.name == key) { return mapping.field; }
    }
    return meminfo_field::unrecognized;
}

/// Reports whether a recognized field carries a bare count instead of a
/// kibibyte amount.
inline bool is_bare_count_field(meminfo_field field) noexcept {
    return field == meminfo_field::huge_total_count ||
           field == meminfo_field::huge_free_count;
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
        const meminfo_field field =
            classify_meminfo_key(trim_memory_field(line.substr(0U, separator)));
        if (field == meminfo_field::unrecognized) { continue; }

        const std::string_view amount = line.substr(separator + 1U);
        result<std::uint64_t> value = is_bare_count_field(field)
                                          ? parse_bare_count(amount)
                                          : parse_kilobyte_amount(amount);
        if (!value) { return fail(value.error()); }

        switch (field) {
        case meminfo_field::total:
            output.total_bytes = *value;
            output.has_total = true;
            break;
        case meminfo_field::available:
            output.available_bytes = *value;
            output.has_available = true;
            break;
        case meminfo_field::swap_total:
            output.swap_total_bytes = *value;
            output.has_swap_total = true;
            break;
        case meminfo_field::swap_free:
            output.swap_free_bytes = *value;
            output.has_swap_free = true;
            break;
        case meminfo_field::committed:
            output.committed_bytes = *value;
            output.has_committed = true;
            break;
        case meminfo_field::commit_limit:
            output.commit_limit_bytes = *value;
            output.has_commit_limit = true;
            break;
        case meminfo_field::huge_total_count:
            output.huge_total_count = *value;
            output.has_huge_total = true;
            break;
        case meminfo_field::huge_free_count:
            output.huge_free_count = *value;
            output.has_huge_free = true;
            break;
        case meminfo_field::huge_page_size:
            output.huge_page_size_bytes = *value;
            output.has_huge_page_size = true;
            break;
        case meminfo_field::unrecognized:
            break;
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

inline result<memory_common::commit_usage> commit_status() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    if (!information->has_committed || !information->has_commit_limit) {
        return fail(errc::not_found);
    }
    memory_common::commit_usage usage;
    usage.committed_bytes = information->committed_bytes;
    usage.commit_limit_bytes = information->commit_limit_bytes;
    return usage;
}

inline result<std::uint64_t> huge_page_size_bytes() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    if (!information->has_huge_page_size) { return fail(errc::not_found); }
    return information->huge_page_size_bytes;
}

inline result<memory_common::huge_page_pool_usage> huge_page_pool_status() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    if (!information->has_huge_total || !information->has_huge_free) {
        return fail(errc::not_found);
    }
    memory_common::huge_page_pool_usage pool;
    pool.total_count = information->huge_total_count;
    pool.free_count = information->huge_free_count;
    return pool;
}

inline result<std::uint32_t> memory_load_percent() {
    const result<memory_information> information = read_memory_information();
    if (!information) { return fail(information.error()); }
    // The load estimate fundamentally depends on the platform's
    // allocatable-without-swapping estimate; kernels without MemAvailable
    // therefore report not_supported rather than a different definition.
    if (!information->has_available) { return fail(errc::not_supported); }
    if (!information->has_total) { return fail(errc::not_found); }
    if (information->available_bytes > information->total_bytes) {
        return fail(errc::malformed_data);
    }
    return memory_common::utilization_percent(
        information->total_bytes - information->available_bytes,
        information->total_bytes);
}

/// Parses one pressure-stall percentage such as "12.34" into micro-percent.
///
/// The kernel renders pressure averages with one to three integer digits and
/// exactly two fractional digits. Any other shape is malformed platform data
/// rather than a rounded or truncated reading, and renderings beyond the
/// documented 100-percent bound are contradictory data.
inline result<std::uint64_t> parse_micro_percent(std::string_view input) {
    const std::string_view value = trim_memory_field(input);
    const std::size_t separator = value.find('.');
    if (separator == std::string_view::npos || separator == 0U ||
        separator > 3U || value.size() - separator != 3U) {
        return fail(errc::malformed_data);
    }
    std::uint64_t whole = 0U;
    const char* whole_first = value.data();
    const char* whole_last = whole_first + separator;
    const std::from_chars_result parsed_whole =
        std::from_chars(whole_first, whole_last, whole);
    if (parsed_whole.ec != std::errc() || parsed_whole.ptr != whole_last) {
        return fail(errc::malformed_data);
    }
    std::uint64_t fraction = 0U;
    const char* fraction_first = whole_last + 1U;
    const char* fraction_last = value.data() + value.size();
    const std::from_chars_result parsed_fraction =
        std::from_chars(fraction_first, fraction_last, fraction);
    if (parsed_fraction.ec != std::errc() ||
        parsed_fraction.ptr != fraction_last) {
        return fail(errc::malformed_data);
    }
    const std::uint64_t hundredths = whole * 100U + fraction;
    if (hundredths > 10000U) { return fail(errc::malformed_data); }
    return hundredths * 10000U;
}

/// Parses one documented assignment of a pressure record into its sample.
///
/// The kernel documents four fixed assignments per record: avg10, avg60,
/// and avg300 as percentages, plus total as a plain microsecond count.
/// Unknown extra tokens are skipped defensively; a recognized kind whose
/// line lacks any required assignment is malformed platform data. Duplicate
/// assignments keep the final value, matching sequential reads.
inline result<memory_common::pressure_sample> parse_pressure_sample(
    std::string_view body) {
    memory_common::pressure_sample sample;
    bool has_avg10 = false;
    bool has_avg60 = false;
    bool has_avg300 = false;
    bool has_total = false;
    std::size_t offset = 0U;
    while (offset < body.size()) {
        const std::size_t end = body.find(' ', offset);
        const std::string_view token =
            body.substr(offset, end == std::string_view::npos
                                    ? body.size() - offset
                                    : end - offset);
        offset = end == std::string_view::npos ? body.size() : end + 1U;

        const std::size_t assignment = token.find('=');
        if (assignment == std::string_view::npos) { continue; }
        const std::string_view name = token.substr(0U, assignment);
        const std::string_view text = token.substr(assignment + 1U);

        if (name == "avg10" || name == "avg60" || name == "avg300") {
            const result<std::uint64_t> percent = parse_micro_percent(text);
            if (!percent) { return fail(percent.error()); }
            if (name == "avg10") {
                sample.average10_micro_percent = *percent;
                has_avg10 = true;
            } else if (name == "avg60") {
                sample.average60_micro_percent = *percent;
                has_avg60 = true;
            } else {
                sample.average300_micro_percent = *percent;
                has_avg300 = true;
            }
        } else if (name == "total") {
            const result<std::uint64_t> total = parse_bare_count(text);
            if (!total) { return fail(total.error()); }
            sample.total_microseconds = *total;
            has_total = true;
        }
    }
    if (!has_avg10 || !has_avg60 || !has_avg300 || !has_total) {
        return fail(errc::malformed_data);
    }
    return sample;
}

/// Splits one documented pressure line into its kind token and assignments.
///
/// A recognized kind whose record cannot be parsed fails the snapshot with
/// the parser's error. Unrecognized kinds are skipped so future kernel
/// additions never break existing queries.
inline result<bool> classify_pressure_line(std::string_view line,
                                           memory_common::pressure_status& status,
                                           bool& has_some, bool& has_full) {
    const std::size_t separator = line.find(' ');
    if (separator == std::string_view::npos) { return result<bool>(false); }
    const std::string_view kind = line.substr(0U, separator);
    if (kind != "some" && kind != "full") { return result<bool>(false); }
    const result<memory_common::pressure_sample> sample =
        parse_pressure_sample(line.substr(separator + 1U));
    if (!sample) { return fail(sample.error()); }
    if (kind == "some") {
        status.some = *sample;
        has_some = true;
    } else {
        status.full = *sample;
        has_full = true;
    }
    return result<bool>(true);
}

/// Parses the documented /proc/pressure/memory snapshot.
///
/// The some record is required because it exists on every kernel that
/// exposes the interface at all; the full record became part of the
/// documented format later, so its absence leaves has_full false and the
/// full record zeroed instead of failing.
inline result<memory_common::pressure_status> parse_memory_pressure(
    std::string_view input) {
    memory_common::pressure_status status;
    bool has_some = false;
    bool has_full = false;
    std::size_t offset = 0U;
    while (offset < input.size()) {
        const std::size_t end = input.find('\n', offset);
        const std::string_view line = input.substr(
            offset, end == std::string_view::npos ? input.size() - offset
                                                   : end - offset);
        offset = end == std::string_view::npos ? input.size() : end + 1U;
        if (trim_memory_field(line).empty()) { continue; }
        const result<bool> recognized =
            classify_pressure_line(line, status, has_some, has_full);
        if (!recognized) { return fail(recognized.error()); }
    }
    if (!has_some) { return fail(errc::malformed_data); }
    // The classifier records presence in caller-owned flags; publish them
    // on the returned snapshot before handing it to callers.
    status.has_full = has_full;
    return status;
}

inline result<memory_common::pressure_status> memory_pressure() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/pressure/memory");
    if (!content) {
        // Kernels built without pressure information, or with it
        // administratively disabled, expose no file at all. That absence
        // means the capability is unsupported rather than a failed read;
        // every other failure preserves its native error.
        const std::error_code& code = content.error();
        if (code.category() == std::generic_category() &&
            code.value() == ENOENT) {
            return fail(errc::not_supported);
        }
        return fail(content.error());
    }
    return parse_memory_pressure(*content);
}

} // namespace memory_backend
} // namespace detail
} // namespace syscape

#endif
