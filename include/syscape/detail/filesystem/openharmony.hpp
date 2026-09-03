#ifndef SYSCAPE_DETAIL_FILESYSTEM_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_FILESYSTEM_OPENHARMONY_HPP

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <sys/statvfs.h>
#include <sys/vfs.h>

#include <syscape/detail/filesystem/common.hpp>
#include <syscape/detail/filesystem/posix.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace filesystem_backend {

inline bool is_mount_separator(char value) noexcept {
    return value == ' ' || value == '\t';
}

inline std::vector<std::string_view> split_mount_line(std::string_view line) {
    std::vector<std::string_view> fields;
    std::size_t offset = 0U;
    while (offset < line.size()) {
        while (offset < line.size() && is_mount_separator(line[offset])) {
            ++offset;
        }
        if (offset >= line.size()) {
            break;
        }
        const std::size_t start = offset;
        while (offset < line.size() && !is_mount_separator(line[offset])) {
            ++offset;
        }
        fields.push_back(line.substr(start, offset - start));
    }
    return fields;
}

inline int octal_digit_value(char value) noexcept {
    return value >= '0' && value <= '7' ? value - '0' : -1;
}

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
    case 040:
        return ' ';
    case 011:
        return '\t';
    case 012:
        return '\n';
    case 0134:
        return '\\';
    default:
        return fail(errc::malformed_data);
    }
}

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
        const result<char> decoded = decode_mount_escape(field, offset);
        if (!decoded) {
            return fail(decoded.error());
        }
        output.push_back(*decoded);
        offset += 4U;
    }
    return output;
}

inline result<std::vector<filesystem_common::mount_record>>
parse_mounts(std::string_view input) {
    std::vector<filesystem_common::mount_record> records;
    std::size_t offset = 0U;
    while (offset <= input.size()) {
        if (offset == input.size()) {
            break;
        }
        const std::size_t end = input.find('\n', offset);
        const std::size_t limit =
            end == std::string_view::npos ? input.size() : end;
        const std::vector<std::string_view> fields =
            split_mount_line(input.substr(offset, limit - offset));
        offset = end == std::string_view::npos ? input.size() : end + 1U;

        if (fields.empty()) {
            continue;
        }
        if (fields.size() < 6U) {
            return fail(errc::malformed_data);
        }

        filesystem_common::mount_record record;
        result<std::string> source = decode_mount_field(fields[0U]);
        if (!source) {
            return fail(source.error());
        }
        result<std::string> target = decode_mount_field(fields[1U]);
        if (!target) {
            return fail(target.error());
        }
        result<std::string> type = decode_mount_field(fields[2U]);
        if (!type) {
            return fail(type.error());
        }
        record.source = std::move(*source);
        record.mount_point = std::move(*target);
        record.file_system_type = std::move(*type);
        records.push_back(std::move(record));
    }
    return records;
}

inline result<std::vector<filesystem_common::mount_record>> mounts() {
    const result<std::string> content =
        openharmony::read_text_file("/proc/self/mounts");
    if (!content) {
        return fail(content.error());
    }
    return parse_mounts(*content);
}

inline result<filesystem_common::space_snapshot>
space(const std::string& path) {
    return statvfs_space(path);
}

inline result<filesystem_common::path_length_snapshot>
max_component_length(const std::string& path) {
    return pathconf_limit(path, _PC_NAME_MAX);
}

inline result<filesystem_common::path_length_snapshot>
max_path_length(const std::string& path) {
    return pathconf_limit(path, _PC_PATH_MAX);
}

inline result<std::string> statfs_volume_id(const std::string& path) {
    for (;;) {
        struct ::statfs status {};
        if (::statfs(path.c_str(), &status) == 0) {
            std::uint32_t first = 0U;
            std::uint32_t second = 0U;
            std::memcpy(&first, &status.f_fsid, sizeof(first));
            std::memcpy(&second,
                        reinterpret_cast<const char*>(&status.f_fsid) +
                            sizeof(first),
                        sizeof(second));
            return filesystem_common::render_hex_word_pair(first, second);
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

inline result<std::string> volume_id(const std::string& path) {
    return statfs_volume_id(path);
}

} // namespace filesystem_backend
} // namespace detail
} // namespace syscape

#endif
