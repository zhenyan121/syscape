#ifndef SYSCAPE_DETAIL_PRINTER_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_PRINTER_DRAGONFLY_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/printer/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/printer.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_backend {

class dragonfly_file_descriptor {
    public:
    explicit dragonfly_file_descriptor(int value) noexcept : value_(value) {}
    dragonfly_file_descriptor(const dragonfly_file_descriptor&) = delete;
    dragonfly_file_descriptor&
    operator=(const dragonfly_file_descriptor&) = delete;
    ~dragonfly_file_descriptor() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    private:
    int value_;
};

inline result<std::string>
read_text_file(const char* path, std::size_t maximum_size = 1024U * 1024U) {
    const int descriptor = ::open(path, O_RDONLY);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const dragonfly_file_descriptor owned_descriptor(descriptor);
    static_cast<void>(owned_descriptor);

    char buffer[4096];
    std::string output;
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (count == 0) {
            break;
        }
        if (output.size() + static_cast<std::size_t>(count) > maximum_size) {
            return fail(errc::value_too_large);
        }
        output.append(buffer, static_cast<std::size_t>(count));
    }
    return output;
}

inline result<std::vector<::syscape::printer::printer_info>>
parse_printcap_content(std::string_view content) {
    std::vector<::syscape::printer::printer_info> printers;
    std::size_t line_start = 0U;
    std::string entry_text;

    while (line_start < content.size()) {
        const std::size_t line_end = content.find('\n', line_start);
        std::string_view line =
            (line_end == std::string_view::npos)
                ? content.substr(line_start)
                : content.substr(line_start, line_end - line_start);
        line_start = (line_end == std::string_view::npos) ? content.size()
                                                          : line_end + 1U;

        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1U);
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }

        if (line.back() == '\\') {
            line.remove_suffix(1U);
            entry_text.append(line);
        } else {
            entry_text.append(line);
            const std::size_t colon_pos = entry_text.find(':');
            if (colon_pos != std::string::npos && colon_pos > 0) {
                const std::string names = entry_text.substr(0, colon_pos);
                std::size_t bar_pos = names.find('|');
                std::string primary_name = (bar_pos == std::string::npos)
                                               ? names
                                               : names.substr(0, bar_pos);
                while (!primary_name.empty() && primary_name.front() == ' ') {
                    primary_name.erase(primary_name.begin());
                }
                while (!primary_name.empty() && primary_name.back() == ' ') {
                    primary_name.pop_back();
                }
                if (!primary_name.empty()) {
                    if (!is_valid_utf8(primary_name)) {
                        return fail(errc::invalid_encoding);
                    }
                    ::syscape::printer::printer_info info;
                    info.id = primary_name;
                    info.name = primary_name;
                    info.type = ::syscape::printer::printer_type::unknown;
                    info.state = ::syscape::printer::printer_state::unknown;
                    printers.push_back(std::move(info));
                }
            }
            entry_text.clear();
        }
    }
    return printers;
}

inline result<std::vector<::syscape::printer::printer_info>>
parse_printcap_file(const char* path) {
    const auto content = read_text_file(path);
    if (!content) {
        return fail(content.error());
    }
    return parse_printcap_content(*content);
}

inline result<std::vector<::syscape::printer::printer_info>> printers() {
    auto pcap = parse_printcap_file("/etc/printcap");
    if (pcap) {
        return pcap;
    }
    if (pcap.error() == errc::not_found ||
        (pcap.error().category() == std::generic_category() &&
         pcap.error().value() == ENOENT)) {
        return std::vector<::syscape::printer::printer_info>();
    }
    return fail(pcap.error());
}

inline result<std::size_t> printer_count() {
    const auto all = printers();
    if (!all) {
        return fail(all.error());
    }
    return all->size();
}

inline result<::syscape::printer::printer_info> default_printer() {
    return fail(errc::not_supported);
}

inline result<::syscape::printer::printer_info>
find_printer(std::string_view name_or_id) {
    const auto all = printers();
    if (!all) {
        return fail(all.error());
    }
    for (const auto& p : *all) {
        if (p.id == name_or_id || p.name == name_or_id) {
            return p;
        }
    }
    return fail(errc::not_found);
}

} // namespace printer_backend
} // namespace detail
} // namespace syscape

#endif
