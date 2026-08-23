#ifndef SYSCAPE_DETAIL_RESOURCE_LINUX_HPP
#define SYSCAPE_DETAIL_RESOURCE_LINUX_HPP

#include <charconv>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

/// Splits an input into whitespace-separated tokens.
///
/// The /proc text formats separate fields with horizontal whitespace:
/// /proc/loadavg uses single spaces while sysctl renderings such as
/// /proc/sys/fs/file-nr use tabs. Fields are never empty, so an empty
/// result means the input is exhausted.
class token_scanner {
public:
    explicit token_scanner(std::string_view input) noexcept : input_(input) {}

    /// Returns the next nonempty token, or an empty view at the end.
    std::string_view next() noexcept {
        while (position_ < input_.size() && separator(input_[position_])) {
            ++position_;
        }
        const std::size_t start = position_;
        while (position_ < input_.size() &&
               !separator(input_[position_])) {
            ++position_;
        }
        return input_.substr(start, position_ - start);
    }

private:
    /// Reports whether a character separates fields.
    static bool separator(char value) noexcept {
        return value == ' ' || value == '\t';
    }

    std::string_view input_;
    std::size_t position_ = 0U;
};

/// Removes the trailing line break that a single /proc read appends.
inline void strip_line_break(std::string_view& input) noexcept {
    while (!input.empty() &&
           (input.back() == '\n' || input.back() == '\r' ||
            input.back() == ' ' || input.back() == '\t')) {
        input.remove_suffix(1U);
    }
}

/// Parses one unsigned decimal token in full.
///
/// A token with any non-digit character is malformed platform data; a value
/// beyond 64 bits reports value_too_large instead of wrapping.
inline result<std::uint64_t> parse_unsigned(std::string_view token) {
    if (token.empty()) { return fail(errc::malformed_data); }
    std::uint64_t value = 0U;
    const char* first = token.data();
    const char* last = first + token.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value);
    if (parsed.ec == std::errc::result_out_of_range) {
        return fail(errc::value_too_large);
    }
    if (parsed.ec != std::errc() ||
        parsed.ptr != last) {
        return fail(errc::malformed_data);
    }
    return value;
}

/// Parses one kernel-rendered load-average sample such as "0.52".
///
/// The kernel renders every sample as digits, one decimal point, then more
/// digits. Samples are dimensionless; zero is valid data for an idle system.
inline result<double> parse_load_sample(std::string_view token) {
    const std::size_t separator = token.find('.');
    if (separator == std::string_view::npos || separator == 0U ||
        separator + 1U >= token.size() ||
        token.find('.', separator + 1U) != std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    const std::string_view whole_text = token.substr(0U, separator);
    const std::string_view fraction_text = token.substr(separator + 1U);
    // Load averages never approach these bounds; longer renderings cannot
    // come from the documented format and guard against pathological inputs.
    if (whole_text.size() > 9U || fraction_text.size() > 9U) {
        return fail(errc::malformed_data);
    }
    const result<std::uint64_t> whole = parse_unsigned(whole_text);
    if (!whole) { return fail(whole.error()); }
    const result<std::uint64_t> fraction = parse_unsigned(fraction_text);
    if (!fraction) { return fail(fraction.error()); }
    double divisor = 1.0;
    for (std::size_t i = 0U; i < fraction_text.size(); ++i) {
        divisor *= 10.0;
    }
    return static_cast<double>(*whole) +
           static_cast<double>(*fraction) / divisor;
}

/// Parsed contents of one /proc/loadavg read.
struct load_status {
    resource_common::load_samples loads;
    resource_common::entity_counts entities;
};

/// Parses the documented five fields of a /proc/loadavg snapshot.
///
/// The kernel documents the format as three load samples, a
/// "running/total" pair of scheduling-entity counts, and the last process
/// identifier assigned. Trailing whitespace is tolerated; any other extra
/// content is malformed platform data.
inline result<load_status> parse_loadavg(std::string_view input) {
    strip_line_break(input);
    token_scanner scanner(input);

    load_status status;
    const result<double> one = parse_load_sample(scanner.next());
    if (!one) { return fail(one.error()); }
    const result<double> five = parse_load_sample(scanner.next());
    if (!five) { return fail(five.error()); }
    const result<double> fifteen = parse_load_sample(scanner.next());
    if (!fifteen) { return fail(fifteen.error()); }
    status.loads.one_minute = *one;
    status.loads.five_minute = *five;
    status.loads.fifteen_minute = *fifteen;

    const std::string_view entities = scanner.next();
    const std::size_t divider = entities.find('/');
    if (divider == std::string_view::npos || divider == 0U ||
        divider + 1U >= entities.size()) {
        return fail(errc::malformed_data);
    }
    const result<std::uint64_t> runnable =
        parse_unsigned(entities.substr(0U, divider));
    if (!runnable) { return fail(runnable.error()); }
    const result<std::uint64_t> schedulable =
        parse_unsigned(entities.substr(divider + 1U));
    if (!schedulable) { return fail(schedulable.error()); }
    status.entities.runnable = *runnable;
    status.entities.schedulable = *schedulable;

    // The final field records the last allocated process identifier. Its
    // value is irrelevant here, but its presence and shape confirm that the
    // record came from the documented format.
    const result<std::uint64_t> last_process =
        parse_unsigned(scanner.next());
    if (!last_process) { return fail(last_process.error()); }

    if (!scanner.next().empty()) { return fail(errc::malformed_data); }
    return status;
}

/// Parsed contents of one /proc/sys/fs/file-nr read.
struct file_table_status {
    /// Number of allocated file handles. Zero is valid data.
    std::uint64_t allocated = 0U;
    /// System-wide maximum number of file handles.
    std::uint64_t maximum = 0U;
};

/// Parses the documented three values of a /proc/sys/fs/file-nr snapshot.
///
/// The first value is the number of allocated file handles, the second is a
/// legacy free-count field, and the third is the system-wide maximum. Only
/// those three values may follow one another.
inline result<file_table_status> parse_file_nr(std::string_view input) {
    strip_line_break(input);
    token_scanner scanner(input);

    file_table_status status;
    const result<std::uint64_t> allocated = parse_unsigned(scanner.next());
    if (!allocated) { return fail(allocated.error()); }
    const result<std::uint64_t> free_handles = parse_unsigned(scanner.next());
    if (!free_handles) { return fail(free_handles.error()); }
    const result<std::uint64_t> maximum = parse_unsigned(scanner.next());
    if (!maximum) { return fail(maximum.error()); }
    if (!scanner.next().empty()) { return fail(errc::malformed_data); }

    status.allocated = *allocated;
    status.maximum = *maximum;
    return status;
}

/// Extracts the num_threads field from one /proc/[pid]/stat record.
///
/// The second stat field is the process name in parentheses, which may
/// itself contain spaces and parentheses, so parsing locates the closing
/// parenthesis before splitting fields. num_threads is the twentieth
/// documented field, the eighteenth token after the name. A live process
/// always owns at least one thread, so zero is malformed platform data.
inline result<std::uint64_t> parse_thread_count(std::string_view input) {
    strip_line_break(input);
    const std::size_t name_begin = input.find('(');
    if (name_begin == std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    const std::size_t name_end = input.rfind(')');
    if (name_end == std::string_view::npos || name_end < name_begin) {
        return fail(errc::malformed_data);
    }

    constexpr std::size_t thread_field_index = 17U;
    token_scanner scanner(input.substr(name_end + 1U));
    for (std::size_t index = 0U; index < thread_field_index; ++index) {
        if (scanner.next().empty()) { return fail(errc::malformed_data); }
    }
    const result<std::uint64_t> threads = parse_unsigned(scanner.next());
    if (!threads) { return fail(threads.error()); }
    if (*threads == 0U) { return fail(errc::malformed_data); }
    return threads;
}

/// Owns one open directory stream for the duration of an enumeration.
class directory_handle {
public:
    explicit directory_handle(const char* path) noexcept
        : value_(::opendir(path)) {}
    directory_handle(const directory_handle&) = delete;
    directory_handle& operator=(const directory_handle&) = delete;
    ~directory_handle() {
        if (value_ != nullptr) { ::closedir(value_); }
    }

    /// Returns true when the directory opened successfully.
    bool valid() const noexcept { return value_ != nullptr; }
    /// Returns the owned stream.
    ::DIR* get() const noexcept { return value_; }

private:
    ::DIR* value_;
};

/// Reports whether a directory entry names a numeric process identifier.
inline bool numeric_entry(const char* name) noexcept {
    if (name == nullptr || name[0] == '\0') { return false; }
    for (const char* digit = name; *digit != '\0'; ++digit) {
        if (*digit < '0' || *digit > '9') { return false; }
    }
    return true;
}

/// Walks the visible /proc/[pid] entries, invoking the visitor once per
/// process identifier name.
///
/// The visitor reports its own outcome: a success or a benign skip passes,
/// while any error aborts the walk and propagates unchanged, so permission
/// and input failures can never masquerade as complete results. Returns the
/// number of entries passed to the visitor.
template <typename Visit>
inline result<std::uint64_t> walk_processes(Visit visit) {
    directory_handle directory("/proc");
    if (!directory.valid()) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::uint64_t visited = 0U;
    for (;;) {
        errno = 0;
        const ::dirent* entry = ::readdir(directory.get());
        if (entry == nullptr) { break; }
        if (!numeric_entry(entry->d_name)) { continue; }
        if (visited == (std::numeric_limits<std::uint64_t>::max)()) {
            return fail(errc::value_too_large);
        }
        const result<void> outcome = visit(entry->d_name);
        if (!outcome) { return fail(outcome.error()); }
        ++visited;
    }
    if (errno != 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    return visited;
}

inline result<resource_common::load_samples> load_average() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/loadavg");
    if (!content) { return fail(content.error()); }
    const result<load_status> status = parse_loadavg(*content);
    if (!status) { return fail(status.error()); }
    return status->loads;
}

inline result<resource_common::entity_counts> scheduler_entities() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/loadavg");
    if (!content) { return fail(content.error()); }
    const result<load_status> status = parse_loadavg(*content);
    if (!status) { return fail(status.error()); }
    return status->entities;
}

inline result<std::uint64_t> process_count() {
    return walk_processes([](const char*) { return result<void>{}; });
}

inline result<std::uint64_t> thread_count() {
    std::uint64_t total_threads = 0U;
    bool overflowed = false;

    const result<std::uint64_t> visited = walk_processes(
        [&](const char* name) -> result<void> {
            const std::string path =
                std::string("/proc/") + name + "/stat";
            const result<std::string> content =
                linux_platform::read_text_file(path.c_str());
            if (!content) {
                // Between listing and reading, the process can exit: its
                // directory entry then disappears and the read fails with
                // ENOENT. Only that documented race is skippable;
                // permission, input, and other native failures propagate.
                if (content.error() ==
                    std::error_code(ENOENT, std::generic_category())) {
                    return {};
                }
                return fail(content.error());
            }
            // A real stat record always contains at least a name, so an
            // empty successful read also describes a record whose process
            // exited mid-read.
            if (content->empty()) { return {}; }
            const result<std::uint64_t> threads =
                parse_thread_count(*content);
            if (!threads) {
                // A complete record that no longer matches the kernel's
                // documented format is malformed platform data.
                return fail(threads.error());
            }
            if (total_threads >
                (std::numeric_limits<std::uint64_t>::max)() - *threads) {
                overflowed = true;
                return {};
            }
            total_threads += *threads;
            return {};
        });
    if (!visited) { return fail(visited.error()); }
    if (overflowed) { return fail(errc::value_too_large); }
    return total_threads;
}

inline result<file_table_status> read_file_table_status() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/sys/fs/file-nr");
    if (!content) { return fail(content.error()); }
    return parse_file_nr(*content);
}

inline result<std::uint64_t> open_file_count() {
    const result<file_table_status> status = read_file_table_status();
    if (!status) { return fail(status.error()); }
    return status->allocated;
}

/// Returns not_supported because Linux documents no system-wide total of
/// open kernel-object handles.
///
/// The file-nr sysctl counts allocated file handles only, and reporting
/// that population as an all-handles total would misstate it.
inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    const result<file_table_status> status = read_file_table_status();
    if (!status) { return fail(status.error()); }
    return status->maximum;
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
