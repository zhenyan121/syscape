#ifndef SYSCAPE_DETAIL_FILESYSTEM_LINUX_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_LINUX_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

/// Returns whether a character separates two /proc/mounts fields.
///
/// Field values encode spaces and tabs as octal escapes, so every literal
/// separator in the raw text starts a new field.
inline bool is_mount_separator(char value) noexcept {
    return value == ' ' || value == '\t';
}

/// Splits one raw /proc/mounts line into its whitespace-separated fields.
inline std::vector<std::string_view> split_mount_line(
    std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t offset = 0U;
    while (offset < line.size()) {
        while (offset < line.size() && is_mount_separator(line[offset])) {
            ++offset;
        }
        if (offset >= line.size()) { break; }
        const std::size_t start = offset;
        while (offset < line.size() && !is_mount_separator(line[offset])) {
            ++offset;
        }
        fields.push_back(line.substr(start, offset - start));
    }
    return fields;
}

/// Returns the octal digit value of a character, or -1 for non-octal text.
inline int octal_digit_value(char value) noexcept {
    return value >= '0' && value <= '7' ? value - '0' : -1;
}

/// Decodes one octal escape sequence of the documented /proc/mounts form.
///
/// The kernel escapes exactly space (\040), tab (\011), newline (\012), and
/// backslash (\134). Every other backslash sequence, including a truncated
/// escape at the end of a field, is malformed platform data because no
/// documented producer emits it.
inline result<char> decode_mount_escape(std::string_view field,
                                        std::size_t backslash) {
    if (field.size() - backslash < 4U) {
        return fail(errc::malformed_data);
    }
    const int hundreds = octal_digit_value(field[backslash + 1U]);
    const int tens = octal_digit_value(field[backslash + 2U]);
    const int ones = octal_digit_value(field[backslash + 3U]);
    if (hundreds < 0 || tens < 0 || ones < 0) {
        return fail(errc::malformed_data);
    }

    switch (hundreds * 64 + tens * 8 + ones) {
    case 040: return ' ';
    case 011: return '\t';
    case 012: return '\n';
    case 0134: return '\\';
    default: return fail(errc::malformed_data);
    }
}

/// Decodes a device, mount-point, or type field reported by the kernel.
inline result<std::string> decode_mount_field(std::string_view field) {
    std::string output;
    output.reserve(field.size());
    std::size_t offset = 0U;
    while (offset < field.size()) {
        if (field[offset] != '\\') {
            output.push_back(field[offset]);
            ++offset;
            continue;
        }
        result<char> decoded = decode_mount_escape(field, offset);
        if (!decoded) { return fail(decoded.error()); }
        output.push_back(*decoded);
        offset += 4U;
    }
    return output;
}

/// Parses a /proc/self/mounts snapshot into mount records.
///
/// The kernel documents each record as six whitespace-separated fields:
/// device, mount point, file-system type, options, dump frequency, and fsck
/// order. Records keep their first three fields after octal decoding; the
/// remaining fields are not needed by this slice. Empty lines are skipped.
/// A record with fewer than six fields is malformed platform data, and so
/// is any record whose required fields carry an undocumented escape.
/// Because field values escape their own separators, decoding never
/// reintroduces ambiguity between records.
inline result<std::vector<filesystem_common::mount_record>> parse_mounts(
    std::string_view input) {
    std::vector<filesystem_common::mount_record> records;
    std::size_t offset = 0U;
    while (offset <= input.size()) {
        if (offset == input.size()) { break; }
        const std::size_t end = input.find('\n', offset);
        const std::size_t limit =
            end == std::string_view::npos ? input.size() : end;
        const std::vector<std::string_view> fields =
            split_mount_line(input.substr(offset, limit - offset));
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        if (fields.empty()) { continue; }
        // The kernel documents six fields; extra fields would belong to a
        // future format extension and are ignored rather than fatal.
        if (fields.size() < 6U) { return fail(errc::malformed_data); }

        filesystem_common::mount_record record;
        result<std::string> source = decode_mount_field(fields[0U]);
        if (!source) { return fail(source.error()); }
        result<std::string> target = decode_mount_field(fields[1U]);
        if (!target) { return fail(target.error()); }
        result<std::string> type = decode_mount_field(fields[2U]);
        if (!type) { return fail(type.error()); }
        record.source = std::move(*source);
        record.mount_point = std::move(*target);
        record.file_system_type = std::move(*type);
        records.push_back(std::move(record));
    }
    return records;
}

/// Reads the live mount table through the kernel's per-process view.
inline result<std::vector<filesystem_common::mount_record>> mounts() {
    const result<std::string> content =
        linux_platform::read_text_file("/proc/self/mounts");
    if (!content) { return fail(content.error()); }
    return parse_mounts(*content);
}

/// Queries capacity for the volume containing the given path.
///
/// Path input is validated at the public boundary before backend selection.
inline result<filesystem_common::space_snapshot> space(
    const std::string& path) {
    return statvfs_space(path);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
