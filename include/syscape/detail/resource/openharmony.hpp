#ifndef SYSCAPE_DETAIL_RESOURCE_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_RESOURCE_OPENHARMONY_HPP

#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <system_error>

#include <syscape/detail/openharmony/directory.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/resource/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace resource_backend {

inline void strip_resource_line_break(std::string_view& input) noexcept {
    while (!input.empty() && (input.back() == '\n' || input.back() == '\r' ||
                              input.back() == ' ' || input.back() == '\t')) {
        input.remove_suffix(1U);
    }
}

inline result<std::uint64_t> parse_unsigned_res(std::string_view token) {
    if (token.empty()) {
        return fail(errc::malformed_data);
    }
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

inline result<double> parse_load_val(std::string_view token) {
    const std::size_t sep = token.find('.');
    if (sep == std::string_view::npos || sep == 0U ||
        sep + 1U >= token.size()) {
        return fail(errc::malformed_data);
    }
    const result<std::uint64_t> whole =
        parse_unsigned_res(token.substr(0U, sep));
    if (!whole) {
        return fail(whole.error());
    }
    const result<std::uint64_t> frac =
        parse_unsigned_res(token.substr(sep + 1U));
    if (!frac) {
        return fail(frac.error());
    }
    double div = 1.0;
    for (std::size_t i = 0U; i < token.size() - sep - 1U; ++i) {
        div *= 10.0;
    }
    return static_cast<double>(*whole) + static_cast<double>(*frac) / div;
}

inline result<resource_common::load_samples> load_average() {
    const result<std::string> content =
        openharmony::read_text_file("/proc/loadavg");
    if (!content) {
        if (content.error() == errc::permission_denied) {
            return fail(errc::permission_denied);
        }
        return fail(content.error());
    }

    std::string_view input = *content;
    strip_resource_line_break(input);
    std::size_t pos = 0U;
    auto next_tok = [&]() -> std::string_view {
        while (pos < input.size() &&
               (input[pos] == ' ' || input[pos] == '\t')) {
            ++pos;
        }
        const std::size_t start = pos;
        while (pos < input.size() && input[pos] != ' ' && input[pos] != '\t') {
            ++pos;
        }
        return input.substr(start, pos - start);
    };

    const auto t1 = next_tok();
    const auto t2 = next_tok();
    const auto t3 = next_tok();
    const auto l1 = parse_load_val(t1);
    const auto l2 = parse_load_val(t2);
    const auto l3 = parse_load_val(t3);
    if (!l1 || !l2 || !l3) {
        return fail(errc::malformed_data);
    }

    resource_common::load_samples samples {};
    samples.one_minute = *l1;
    samples.five_minute = *l2;
    samples.fifteen_minute = *l3;
    return samples;
}

inline result<resource_common::entity_counts> scheduler_entities() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> process_count() {
    openharmony::directory_handle dir("/proc");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }
    std::uint64_t count = 0U;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] >= '0' && entry->d_name[0] <= '9') {
            bool all_digits = true;
            for (const char* p = entry->d_name; *p != '\0'; ++p) {
                if (*p < '0' || *p > '9') {
                    all_digits = false;
                    break;
                }
            }
            if (all_digits) {
                ++count;
            }
        }
    }
    return count;
}

inline result<std::uint64_t> thread_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_file_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> open_handle_count() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> file_descriptor_limit() {
    const result<std::string> content =
        openharmony::read_text_file("/proc/sys/fs/file-max");
    if (content) {
        std::string_view val = *content;
        strip_resource_line_break(val);
        return parse_unsigned_res(val);
    }
    if (content.error() != errc::not_found) {
        return fail(content.error());
    }

    const result<std::string> nr_content =
        openharmony::read_text_file("/proc/sys/fs/file-nr");
    if (nr_content) {
        std::string_view val = *nr_content;
        strip_resource_line_break(val);
        const std::size_t last_sep = val.find_last_of(" \t");
        if (last_sep == std::string_view::npos) {
            return fail(errc::malformed_data);
        }
        return parse_unsigned_res(val.substr(last_sep + 1U));
    }
    return fail(nr_content.error());
}

} // namespace resource_backend
} // namespace detail
} // namespace syscape

#endif
