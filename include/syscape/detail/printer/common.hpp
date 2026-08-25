#ifndef SYSCAPE_DETAIL_PRINTER_COMMON_HPP
#define SYSCAPE_DETAIL_PRINTER_COMMON_HPP

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_common {

/// Trims leading and trailing ASCII whitespace.
inline std::string_view trim_whitespace(std::string_view text) noexcept {
    while (!text.empty() && static_cast<unsigned char>(text.front()) <= ' ') {
        text.remove_prefix(1U);
    }
    while (!text.empty() && static_cast<unsigned char>(text.back()) <= ' ') {
        text.remove_suffix(1U);
    }
    return text;
}

/// Case-insensitive ASCII string comparison.
inline bool equals_ignore_case(std::string_view left,
                               std::string_view right) noexcept {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(left[i])) !=
            std::tolower(static_cast<unsigned char>(right[i]))) {
            return false;
        }
    }
    return true;
}

/// Case-insensitive prefix check.
inline bool starts_with_ignore_case(std::string_view text,
                                    std::string_view prefix) noexcept {
    if (text.size() < prefix.size()) {
        return false;
    }
    return equals_ignore_case(text.substr(0, prefix.size()), prefix);
}

/// Case-insensitive ASCII substring check.
inline bool contains_ignore_case(std::string_view text,
                                 std::string_view part) noexcept {
    if (part.empty()) { return true; }
    if (part.size() > text.size()) { return false; }
    for (std::size_t i = 0U; i + part.size() <= text.size(); ++i) {
        if (equals_ignore_case(text.substr(i, part.size()), part)) {
            return true;
        }
    }
    return false;
}

/// Normalizes a queue name, display name, or USB URI for physical-identity
/// comparisons. ASCII punctuation and URI query parameters are ignored, and
/// percent-encoded unreserved bytes are decoded.
inline std::string normalized_printer_identity(std::string_view text) {
    if (starts_with_ignore_case(text, "usb://")) { text.remove_prefix(6U); }
    const auto query = text.find('?');
    if (query != std::string_view::npos) { text = text.substr(0U, query); }

    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0U; i < text.size(); ++i) {
        unsigned char byte = static_cast<unsigned char>(text[i]);
        if (byte == '%' && i + 2U < text.size()) {
            const auto hex_value = [](unsigned char c) -> int {
                if (c >= '0' && c <= '9') { return c - '0'; }
                if (c >= 'a' && c <= 'f') { return c - 'a' + 10; }
                if (c >= 'A' && c <= 'F') { return c - 'A' + 10; }
                return -1;
            };
            const int high = hex_value(static_cast<unsigned char>(text[i + 1U]));
            const int low = hex_value(static_cast<unsigned char>(text[i + 2U]));
            if (high >= 0 && low >= 0) {
                byte = static_cast<unsigned char>((high << 4) | low);
                i += 2U;
            }
        }
        if ((byte >= 'a' && byte <= 'z') ||
            (byte >= '0' && byte <= '9') || byte >= 0x80U) {
            normalized.push_back(static_cast<char>(byte));
        } else if (byte >= 'A' && byte <= 'Z') {
            normalized.push_back(static_cast<char>(byte - 'A' + 'a'));
        }
    }
    return normalized;
}

/// Compares text naturally so that decimal digit runs sort by numeric value.
inline bool natural_less(std::string_view left,
                         std::string_view right) noexcept {
    std::size_t left_pos = 0U;
    std::size_t right_pos = 0U;
    while (left_pos < left.size() && right_pos < right.size()) {
        const bool left_digit = left[left_pos] >= '0' && left[left_pos] <= '9';
        const bool right_digit =
            right[right_pos] >= '0' && right[right_pos] <= '9';
        if (!left_digit || !right_digit) {
            if (left[left_pos] != right[right_pos]) {
                return left[left_pos] < right[right_pos];
            }
            ++left_pos;
            ++right_pos;
            continue;
        }

        const std::size_t left_run_begin = left_pos;
        const std::size_t right_run_begin = right_pos;
        while (left_pos < left.size() && left[left_pos] == '0') {
            ++left_pos;
        }
        while (right_pos < right.size() && right[right_pos] == '0') {
            ++right_pos;
        }
        const std::size_t left_significant = left_pos;
        const std::size_t right_significant = right_pos;
        while (left_pos < left.size() && left[left_pos] >= '0' &&
               left[left_pos] <= '9') {
            ++left_pos;
        }
        while (right_pos < right.size() && right[right_pos] >= '0' &&
               right[right_pos] <= '9') {
            ++right_pos;
        }

        const std::size_t left_digits = left_pos - left_significant;
        const std::size_t right_digits = right_pos - right_significant;
        if (left_digits != right_digits) {
            return left_digits < right_digits;
        }
        const int numeric_order =
            left.substr(left_significant, left_digits)
                .compare(right.substr(right_significant, right_digits));
        if (numeric_order != 0) {
            return numeric_order < 0;
        }
        const std::size_t left_leading_zeros =
            left_significant - left_run_begin;
        const std::size_t right_leading_zeros =
            right_significant - right_run_begin;
        if (left_leading_zeros != right_leading_zeros) {
            return left_leading_zeros < right_leading_zeros;
        }
    }
    return left_pos >= left.size() && right_pos < right.size();
}

/// Parses an unsigned integer in the given radix.
template <typename IntType>
inline std::optional<IntType> parse_int(std::string_view text,
                                        int base = 10) noexcept {
    text = trim_whitespace(text);
    if (base == 16) {
        if (text.size() >= 2U && text[0] == '0' &&
            (text[1] == 'x' || text[1] == 'X')) {
            text.remove_prefix(2U);
        }
    }
    if (text.empty()) {
        return std::nullopt;
    }
    IntType value = 0;
    const auto* const begin = text.data();
    const auto* const end = begin + text.size();
    const auto [ptr, ec] = std::from_chars(begin, end, value, base);
    if (ec == std::errc() && ptr == end) {
        return value;
    }
    return std::nullopt;
}

/// Copies platform text after validating that it is UTF-8.
inline result<std::string> validate_utf8(std::string_view text) {
    if (!detail::is_valid_utf8(text)) {
        return fail(errc::invalid_encoding);
    }
    return std::string(text);
}

/// Decodes an HTTP response carrying an IPP payload.
inline result<std::vector<std::uint8_t>>
parse_http_ipp_response(const std::vector<std::uint8_t>& response) {
    const auto find_crlf = [&](std::size_t begin) -> std::optional<std::size_t> {
        for (std::size_t i = begin; i + 1U < response.size(); ++i) {
            if (response[i] == '\r' && response[i + 1U] == '\n') {
                return i;
            }
        }
        return std::nullopt;
    };
    const auto view = [&](std::size_t begin, std::size_t length) {
        return std::string_view(
            reinterpret_cast<const char*>(response.data() + begin), length);
    };

    std::size_t message_begin = 0U;
    for (;;) {
        const auto status_end = find_crlf(message_begin);
        if (!status_end) {
            return fail(errc::malformed_data);
        }
        const std::string_view status_line =
            view(message_begin, *status_end - message_begin);
        if (!starts_with_ignore_case(status_line, "HTTP/") ||
            status_line.size() < 12U) {
            return fail(errc::malformed_data);
        }
        const std::size_t first_space = status_line.find(' ');
        if (first_space == std::string_view::npos ||
            first_space + 4U > status_line.size() ||
            (first_space + 4U < status_line.size() &&
             status_line[first_space + 4U] != ' ')) {
            return fail(errc::malformed_data);
        }
        const auto status =
            parse_int<unsigned>(status_line.substr(first_space + 1U, 3U));
        if (!status || *status < 100U || *status > 599U) {
            return fail(errc::malformed_data);
        }

        bool chunked = false;
        std::optional<std::size_t> content_length;
        std::size_t line_begin = *status_end + 2U;
        for (;;) {
            const auto line_end = find_crlf(line_begin);
            if (!line_end) {
                return fail(errc::malformed_data);
            }
            if (*line_end == line_begin) {
                line_begin += 2U;
                break;
            }
            const std::string_view line = view(line_begin, *line_end - line_begin);
            const std::size_t colon = line.find(':');
            if (colon == std::string_view::npos) {
                return fail(errc::malformed_data);
            }
            const std::string_view name = trim_whitespace(line.substr(0U, colon));
            const std::string_view value = trim_whitespace(line.substr(colon + 1U));
            if (name.empty()) { return fail(errc::malformed_data); }
            if (equals_ignore_case(name, "Content-Length")) {
                const auto parsed = parse_int<std::size_t>(value);
                if (!parsed || (content_length && *content_length != *parsed)) {
                    return fail(errc::malformed_data);
                }
                content_length = parsed;
            } else if (equals_ignore_case(name, "Transfer-Encoding")) {
                if (!equals_ignore_case(value, "chunked")) {
                    return fail(errc::not_supported);
                }
                chunked = true;
            }
            line_begin = *line_end + 2U;
        }

        if (*status >= 100U && *status < 200U) {
            if (chunked || (content_length && *content_length != 0U)) {
                return fail(errc::malformed_data);
            }
            message_begin = line_begin;
            if (message_begin >= response.size()) {
                return fail(errc::malformed_data);
            }
            continue;
        }
        if (*status == 401U || *status == 403U) {
            return fail(errc::permission_denied);
        }
        if (*status == 404U) {
            return fail(errc::not_found);
        }
        if (*status == 501U) {
            return fail(errc::not_supported);
        }
        if (*status == 408U || *status == 429U || *status == 503U) {
            return fail(errc::temporarily_unavailable);
        }
        if (*status < 200U || *status >= 300U) {
            return fail(errc::io_error);
        }

        if (!chunked) {
            const std::size_t available = response.size() - line_begin;
            if (content_length && available != *content_length) {
                return fail(errc::malformed_data);
            }
            return std::vector<std::uint8_t>(
                response.begin() + static_cast<std::ptrdiff_t>(line_begin),
                response.end());
        }
        if (content_length) {
            return fail(errc::malformed_data);
        }

        std::vector<std::uint8_t> decoded;
        std::size_t chunk_begin = line_begin;
        for (;;) {
            const auto chunk_line_end = find_crlf(chunk_begin);
            if (!chunk_line_end) {
                return fail(errc::malformed_data);
            }
            std::string_view size_text =
                trim_whitespace(view(chunk_begin, *chunk_line_end - chunk_begin));
            const std::size_t extension = size_text.find(';');
            if (extension != std::string_view::npos) {
                size_text = trim_whitespace(size_text.substr(0U, extension));
            }
            const auto chunk_size = parse_int<std::size_t>(size_text, 16);
            if (!chunk_size) {
                return fail(errc::malformed_data);
            }
            chunk_begin = *chunk_line_end + 2U;
            if (*chunk_size == 0U) {
                for (;;) {
                    const auto trailer_end = find_crlf(chunk_begin);
                    if (!trailer_end) {
                        return fail(errc::malformed_data);
                    }
                    if (*trailer_end == chunk_begin) {
                        chunk_begin += 2U;
                        break;
                    }
                    const std::string_view trailer =
                        view(chunk_begin, *trailer_end - chunk_begin);
                    if (trailer.find(':') == std::string_view::npos) {
                        return fail(errc::malformed_data);
                    }
                    chunk_begin = *trailer_end + 2U;
                }
                if (chunk_begin != response.size()) {
                    return fail(errc::malformed_data);
                }
                return decoded;
            }
            if (*chunk_size > response.size() - chunk_begin ||
                response.size() - chunk_begin - *chunk_size < 2U) {
                return fail(errc::malformed_data);
            }
            decoded.insert(decoded.end(),
                           response.begin() + static_cast<std::ptrdiff_t>(chunk_begin),
                           response.begin() + static_cast<std::ptrdiff_t>(chunk_begin + *chunk_size));
            chunk_begin += *chunk_size;
            if (response[chunk_begin] != '\r' || response[chunk_begin + 1U] != '\n') {
                return fail(errc::malformed_data);
            }
            chunk_begin += 2U;
        }
    }
}

/// Classifies printer connection type based on device URI, printer name, and driver name.
inline syscape::printer::printer_type
classify_printer_type(std::string_view uri,
                      std::string_view name,
                      std::string_view driver) noexcept {
    // Check virtual printer keywords
    if (starts_with_ignore_case(uri, "cups-pdf:") ||
        starts_with_ignore_case(uri, "cups-pdf://") ||
        starts_with_ignore_case(uri, "pdf:") ||
        starts_with_ignore_case(uri, "file:") ||
        starts_with_ignore_case(uri, "PORTPROMPT:") ||
        starts_with_ignore_case(uri, "nul:") ||
        contains_ignore_case(name, "PDF") ||
        contains_ignore_case(name, "XPS") ||
        contains_ignore_case(name, "Fax") ||
        contains_ignore_case(name, "Document Writer") ||
        contains_ignore_case(driver, "PDF") ||
        contains_ignore_case(driver, "XPS") ||
        contains_ignore_case(driver, "Fax")) {
        return syscape::printer::printer_type::virtual_printer;
    }

    // Check network printer URI schemes
    if (starts_with_ignore_case(uri, "ipp://") ||
        starts_with_ignore_case(uri, "ipps://") ||
        starts_with_ignore_case(uri, "socket://") ||
        starts_with_ignore_case(uri, "lpd://") ||
        starts_with_ignore_case(uri, "smb://") ||
        starts_with_ignore_case(uri, "dnssd://") ||
        starts_with_ignore_case(uri, "mdns://") ||
        starts_with_ignore_case(uri, "http://") ||
        starts_with_ignore_case(uri, "https://") ||
        starts_with_ignore_case(uri, "wsd://") ||
        starts_with_ignore_case(uri, "IP_") ||
        starts_with_ignore_case(uri, "WSD-")) {
        return syscape::printer::printer_type::network;
    }

    // Check local hardware printer URI schemes
    if (starts_with_ignore_case(uri, "usb://") ||
        starts_with_ignore_case(uri, "parallel://") ||
        starts_with_ignore_case(uri, "/dev/usb/lp") ||
        starts_with_ignore_case(uri, "/dev/lp") ||
        starts_with_ignore_case(uri, "USB") ||
        starts_with_ignore_case(uri, "LPT") ||
        starts_with_ignore_case(uri, "COM")) {
        return syscape::printer::printer_type::local;
    }

    return syscape::printer::printer_type::unknown;
}

/// Maps IPP printer-state integer (RFC 8011) to syscape::printer::printer_state.
inline syscape::printer::printer_state
map_ipp_printer_state(std::int32_t state) noexcept {
    switch (state) {
    case 3: // IPP_PSTATE_IDLE
        return syscape::printer::printer_state::idle;
    case 4: // IPP_PSTATE_PROCESSING
        return syscape::printer::printer_state::processing;
    case 5: // IPP_PSTATE_STOPPED
        return syscape::printer::printer_state::stopped;
    default:
        return syscape::printer::printer_state::unknown;
    }
}

/// Marks exactly the printer matching the effective default name.
inline void set_default_by_name(
    std::vector<syscape::printer::printer_info>& printers,
    std::string_view name) {
    for (auto& item : printers) {
        item.is_default = item.id == name || item.name == name;
    }
}

/// Selects the explicitly marked default without inventing one from count.
inline result<syscape::printer::printer_info> marked_default_printer(
    const std::vector<syscape::printer::printer_info>& printers) {
    for (const auto& item : printers) {
        if (item.is_default.value_or(false)) { return item; }
    }
    return fail(errc::not_found);
}

/// Resolves the default entry from an already successful list snapshot.
inline result<syscape::printer::printer_info> resolve_snapshot_default(
    const std::vector<syscape::printer::printer_info>& printers,
    bool default_known,
    const std::optional<std::error_code>& default_error) {
    if (printers.empty()) { return fail(errc::not_found); }
    const auto selected = marked_default_printer(printers);
    if (selected) { return selected; }
    if (default_error) { return fail(*default_error); }
    if (!default_known) { return fail(errc::not_supported); }
    return fail(errc::not_found);
}

/// IPP constants according to RFC 8010 and RFC 8011.
namespace ipp {

constexpr std::uint16_t OP_CUPS_GET_DEFAULT = 0x4001;
constexpr std::uint16_t OP_CUPS_GET_PRINTERS = 0x4002;

constexpr std::uint8_t TAG_OPERATION_ATTRIBUTES = 0x01;
constexpr std::uint8_t TAG_JOB_ATTRIBUTES = 0x02;
constexpr std::uint8_t TAG_END_OF_ATTRIBUTES = 0x03;
constexpr std::uint8_t TAG_PRINTER_ATTRIBUTES = 0x04;
constexpr std::uint8_t TAG_UNSUPPORTED_ATTRIBUTES = 0x05;

constexpr std::uint8_t TAG_UNSUPPORTED = 0x10;
constexpr std::uint8_t TAG_UNKNOWN = 0x12;
constexpr std::uint8_t TAG_NO_VALUE = 0x13;
constexpr std::uint8_t TAG_INTEGER = 0x21;
constexpr std::uint8_t TAG_BOOLEAN = 0x22;
constexpr std::uint8_t TAG_ENUM = 0x23;
constexpr std::uint8_t TAG_OCTET_STRING = 0x30;
constexpr std::uint8_t TAG_DATETIME = 0x31;
constexpr std::uint8_t TAG_RESOLUTION = 0x32;
constexpr std::uint8_t TAG_RANGE_OF_INTEGER = 0x33;
constexpr std::uint8_t TAG_TEXT_WITH_LANGUAGE = 0x35;
constexpr std::uint8_t TAG_NAME_WITH_LANGUAGE = 0x36;
constexpr std::uint8_t TAG_TEXT_WITHOUT_LANGUAGE = 0x41;
constexpr std::uint8_t TAG_NAME_WITHOUT_LANGUAGE = 0x42;
constexpr std::uint8_t TAG_KEYWORD = 0x44;
constexpr std::uint8_t TAG_URI = 0x45;
constexpr std::uint8_t TAG_URISCHEME = 0x46;
constexpr std::uint8_t TAG_CHARSET = 0x47;
constexpr std::uint8_t TAG_NATURALLANGUAGE = 0x48;
constexpr std::uint8_t TAG_MIMETYPEREAD = 0x49;

/// CUPS printer-type bitmask flags.
constexpr std::uint32_t CUPS_PRINTER_CLASS = 0x0001;
constexpr std::uint32_t CUPS_PRINTER_REMOTE = 0x0002;
constexpr std::uint32_t CUPS_PRINTER_COLOR = 0x0004;
constexpr std::uint32_t CUPS_PRINTER_DUPLEX = 0x0008;
constexpr std::uint32_t CUPS_PRINTER_COPIES = 0x0020;
constexpr std::uint32_t CUPS_PRINTER_COLLATE = 0x0040;
constexpr std::uint32_t CUPS_PRINTER_FAX = 0x0080;
constexpr std::uint32_t CUPS_PRINTER_SCANNER = 0x0200;
constexpr std::uint32_t CUPS_PRINTER_PDF = 0x20000;

/// Appends a 16-bit big-endian integer to a byte buffer.
inline void append_uint16(std::vector<std::uint8_t>& buf,
                          std::uint16_t val) {
    buf.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

/// Appends a 32-bit big-endian integer to a byte buffer.
inline void append_uint32(std::vector<std::uint8_t>& buf,
                          std::uint32_t val) {
    buf.push_back(static_cast<std::uint8_t>((val >> 24) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 16) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>((val >> 8) & 0xFFU));
    buf.push_back(static_cast<std::uint8_t>(val & 0xFFU));
}

/// Appends an IPP attribute with a string value to a byte buffer.
inline void append_string_attr(std::vector<std::uint8_t>& buf,
                               std::uint8_t tag,
                               std::string_view name,
                               std::string_view val) {
    buf.push_back(tag);
    append_uint16(buf, static_cast<std::uint16_t>(name.size()));
    for (char c : name) {
        buf.push_back(static_cast<std::uint8_t>(c));
    }
    append_uint16(buf, static_cast<std::uint16_t>(val.size()));
    for (char c : val) {
        buf.push_back(static_cast<std::uint8_t>(c));
    }
}

/// Requests every printer attribute consumed by the portable parser.
inline void append_requested_attributes(std::vector<std::uint8_t>& req) {
    constexpr std::string_view attributes[] = {
        "printer-name", "printer-info", "printer-location",
        "printer-make-and-model", "device-uri", "printer-state",
        "printer-is-accepting-jobs", "printer-is-shared",
        "queued-job-count", "color-supported", "printer-type",
        "media-supported", "printer-resolution-supported",
        "copies-supported", "sides-supported"};
    bool first = true;
    for (const auto attribute : attributes) {
        append_string_attr(req, TAG_KEYWORD,
                           first ? "requested-attributes" : "", attribute);
        first = false;
    }
}

/// Builds an IPP binary request for CUPS-Get-Printers.
inline std::vector<std::uint8_t> build_get_printers_request(
    std::uint32_t request_id = 1) {
    std::vector<std::uint8_t> req;
    req.reserve(256);

    // IPP Version 2.0
    req.push_back(0x02);
    req.push_back(0x00);

    // Operation ID: CUPS-Get-Printers (0x4002)
    append_uint16(req, OP_CUPS_GET_PRINTERS);

    // Request ID
    append_uint32(req, request_id);

    // Delimiter: Operation Attributes Tag (0x01)
    req.push_back(TAG_OPERATION_ATTRIBUTES);

    // attributes-charset = "utf-8"
    append_string_attr(req, TAG_CHARSET, "attributes-charset", "utf-8");

    // attributes-natural-language = "en"
    append_string_attr(req, TAG_NATURALLANGUAGE, "attributes-natural-language",
                       "en");

    append_requested_attributes(req);

    // End of attributes (0x03)
    req.push_back(TAG_END_OF_ATTRIBUTES);

    return req;
}

/// Builds an IPP binary request for CUPS-Get-Default.
inline std::vector<std::uint8_t> build_get_default_request(
    std::uint32_t request_id = 1) {
    std::vector<std::uint8_t> req;
    req.reserve(256);

    // IPP Version 2.0
    req.push_back(0x02);
    req.push_back(0x00);

    // Operation ID: CUPS-Get-Default (0x4001)
    append_uint16(req, OP_CUPS_GET_DEFAULT);

    // Request ID
    append_uint32(req, request_id);

    // Delimiter: Operation Attributes Tag (0x01)
    req.push_back(TAG_OPERATION_ATTRIBUTES);

    // attributes-charset = "utf-8"
    append_string_attr(req, TAG_CHARSET, "attributes-charset", "utf-8");

    // attributes-natural-language = "en"
    append_string_attr(req, TAG_NATURALLANGUAGE, "attributes-natural-language",
                       "en");

    append_requested_attributes(req);

    // End of attributes (0x03)
    req.push_back(TAG_END_OF_ATTRIBUTES);

    return req;
}

/// Reads a 16-bit big-endian integer from a byte buffer.
inline std::optional<std::uint16_t> read_uint16(const std::uint8_t* data,
                                                std::size_t size,
                                                std::size_t offset) noexcept {
    if (offset + 2U > size) {
        return std::nullopt;
    }
    const auto val = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(data[offset]) << 8) |
        static_cast<std::uint16_t>(data[offset + 1U]));
    return val;
}

/// Reads a 32-bit big-endian integer from a byte buffer.
inline std::optional<std::uint32_t> read_uint32(const std::uint8_t* data,
                                                std::size_t size,
                                                std::size_t offset) noexcept {
    if (offset + 4U > size) {
        return std::nullopt;
    }
    const auto val = static_cast<std::uint32_t>(
        (static_cast<std::uint32_t>(data[offset]) << 24) |
        (static_cast<std::uint32_t>(data[offset + 1U]) << 16) |
        (static_cast<std::uint32_t>(data[offset + 2U]) << 8) |
        static_cast<std::uint32_t>(data[offset + 3U]));
    return val;
}

/// Parses an IPP response payload into a vector of syscape::printer::printer_info.
inline result<std::vector<syscape::printer::printer_info>>
parse_ipp_printers_response(const std::uint8_t* data, std::size_t size) {
    if (size < 8U) {
        return fail(errc::malformed_data);
    }
    if ((data[0] != 1U && data[0] != 2U) || data[1] > 2U) {
        return fail(errc::malformed_data);
    }
    const auto request_id = read_uint32(data, size, 4U);
    if (!request_id || *request_id == 0U || *request_id > 0x7FFFFFFFU) {
        return fail(errc::malformed_data);
    }

    // Header: version (2 bytes), status-code (2 bytes), request-id (4 bytes)
    const auto status_opt = read_uint16(data, size, 2U);
    if (!status_opt) {
        return fail(errc::malformed_data);
    }
    const std::uint16_t status_code = *status_opt;

    // Status code 0x0000 to 0x00FF are successful / informational.
    if (status_code > 0x00FF) {
        if (status_code == 0x0401 || status_code == 0x0402 ||
            status_code == 0x0403) {
            return fail(errc::permission_denied);
        }
        if (status_code == 0x0406) { // not-found
            return fail(errc::not_found);
        }
        if (status_code == 0x0405 || // client-error-timeout
            status_code == 0x0502 || // server-error-service-unavailable
            status_code == 0x0507) { // server-error-busy
            return fail(errc::temporarily_unavailable);
        }
        return fail(errc::io_error);
    }

    std::vector<syscape::printer::printer_info> result_printers;
    std::size_t pos = 8U;

    syscape::printer::printer_info current_printer;
    bool in_printer_group = false;
    bool saw_end_of_attributes = false;
    bool current_group_has_attributes = false;
    std::string current_attr_name;

    auto finish_current_printer = [&]() -> bool {
        if (current_group_has_attributes && current_printer.id.empty() &&
            current_printer.name.empty()) {
            return false;
        }
        if (!current_printer.id.empty() || !current_printer.name.empty()) {
            if (current_printer.id.empty()) { current_printer.id = current_printer.name; }
            if (current_printer.name.empty()) { current_printer.name = current_printer.id; }
            if (current_printer.type == syscape::printer::printer_type::unknown) {
                current_printer.type = classify_printer_type(
                    current_printer.uri, current_printer.name,
                    current_printer.driver_name);
            }
            result_printers.push_back(current_printer);
        }
        current_printer = syscape::printer::printer_info{};
        current_group_has_attributes = false;
        return true;
    };

    while (pos < size) {
        const std::uint8_t tag = data[pos++];
        if (tag == TAG_END_OF_ATTRIBUTES) {
            saw_end_of_attributes = true;
            break;
        }

        // Check if delimiter tag
        if (tag <= 0x0F) {
            if (tag == TAG_PRINTER_ATTRIBUTES) {
                if (in_printer_group && !finish_current_printer()) {
                    return fail(errc::malformed_data);
                }
                in_printer_group = true;
            } else if (tag == TAG_OPERATION_ATTRIBUTES ||
                       tag == TAG_JOB_ATTRIBUTES ||
                       tag == TAG_UNSUPPORTED_ATTRIBUTES) {
                if (in_printer_group) {
                    if (!finish_current_printer()) {
                        return fail(errc::malformed_data);
                    }
                    in_printer_group = false;
                }
            } else {
                return fail(errc::malformed_data);
            }
            current_attr_name.clear();
            continue;
        }

        // Value attribute: name_len (2), name (name_len), val_len (2), val (val_len)
        const auto name_len_opt = read_uint16(data, size, pos);
        if (!name_len_opt) {
            return fail(errc::malformed_data);
        }
        pos += 2U;
        const std::uint16_t name_len = *name_len_opt;

        if (pos + name_len > size) {
            return fail(errc::malformed_data);
        }

        if (name_len > 0U) {
            current_attr_name =
                std::string(reinterpret_cast<const char*>(data + pos), name_len);
            pos += name_len;
        } else if (current_attr_name.empty()) {
            return fail(errc::malformed_data);
        }
        // If name_len == 0, current_attr_name is preserved (1setOf attribute)

        const auto val_len_opt = read_uint16(data, size, pos);
        if (!val_len_opt) {
            return fail(errc::malformed_data);
        }
        pos += 2U;
        const std::uint16_t val_len = *val_len_opt;

        if (pos + val_len > size) {
            return fail(errc::malformed_data);
        }

        const std::uint8_t* const val_data = data + pos;
        pos += val_len;

        if (!in_printer_group) {
            continue;
        }
        current_group_has_attributes = true;

        // Decode attribute value
        if (current_attr_name == "printer-name") {
            const auto text = validate_utf8(
                std::string_view(reinterpret_cast<const char*>(val_data), val_len));
            if (!text) {
                return fail(text.error());
            }
            if (!text->empty()) {
                current_printer.id = *text;
                current_printer.name = *text;
            }
        } else if (current_attr_name == "printer-info") {
            const auto text = validate_utf8(std::string_view(
                reinterpret_cast<const char*>(val_data), val_len));
            if (!text) { return fail(text.error()); }
            current_printer.description = *text;
        } else if (current_attr_name == "printer-location") {
            const auto text = validate_utf8(std::string_view(
                reinterpret_cast<const char*>(val_data), val_len));
            if (!text) { return fail(text.error()); }
            current_printer.location = *text;
        } else if (current_attr_name == "printer-make-and-model") {
            const auto text = validate_utf8(std::string_view(
                reinterpret_cast<const char*>(val_data), val_len));
            if (!text) { return fail(text.error()); }
            current_printer.driver_name = *text;
        } else if (current_attr_name == "device-uri") {
            const auto text = validate_utf8(std::string_view(
                reinterpret_cast<const char*>(val_data), val_len));
            if (!text) { return fail(text.error()); }
            current_printer.uri = *text;
        } else if (current_attr_name == "printer-state") {
            if (val_len != 4U) { return fail(errc::malformed_data); }
            const auto ival = read_uint32(val_data, val_len, 0U);
            if (!ival || *ival > 0x7FFFFFFFU) {
                return fail(errc::malformed_data);
            }
            current_printer.state =
                map_ipp_printer_state(static_cast<std::int32_t>(*ival));
        } else if (current_attr_name == "printer-is-accepting-jobs") {
            if (val_len != 1U || val_data[0] > 1U) {
                return fail(errc::malformed_data);
            }
            current_printer.is_accepting_jobs = (val_data[0] != 0U);
        } else if (current_attr_name == "printer-is-shared") {
            if (val_len != 1U || val_data[0] > 1U) {
                return fail(errc::malformed_data);
            }
            current_printer.is_shared = (val_data[0] != 0U);
        } else if (current_attr_name == "queued-job-count") {
            if (val_len != 4U) { return fail(errc::malformed_data); }
            const auto ival = read_uint32(val_data, val_len, 0U);
            if (!ival || *ival > 0x7FFFFFFFU) {
                return fail(errc::malformed_data);
            }
            current_printer.queued_job_count = ival;
        } else if (current_attr_name == "color-supported") {
            if (val_len != 1U || val_data[0] > 1U) {
                return fail(errc::malformed_data);
            }
            current_printer.capabilities.color = (val_data[0] != 0U);
        } else if (current_attr_name == "printer-type") {
            if (val_len != 4U) { return fail(errc::malformed_data); }
            const auto ival = read_uint32(val_data, val_len, 0U);
            if (!ival) { return fail(errc::malformed_data); }
            const std::uint32_t type_flags = *ival;
            if (type_flags & CUPS_PRINTER_COLOR) {
                current_printer.capabilities.color = true;
            }
            if (type_flags & CUPS_PRINTER_DUPLEX) {
                current_printer.capabilities.duplex = true;
            }
            if (type_flags & CUPS_PRINTER_COPIES) {
                current_printer.capabilities.copies = true;
            }
            if (type_flags & CUPS_PRINTER_COLLATE) {
                current_printer.capabilities.collate = true;
            }
            if ((type_flags & CUPS_PRINTER_FAX) ||
                (type_flags & CUPS_PRINTER_PDF)) {
                current_printer.type =
                    syscape::printer::printer_type::virtual_printer;
            } else if (type_flags & CUPS_PRINTER_REMOTE) {
                current_printer.type =
                    syscape::printer::printer_type::network;
            }
        } else if (current_attr_name == "media-supported") {
            const auto media = validate_utf8(
                std::string_view(reinterpret_cast<const char*>(val_data), val_len));
            if (!media) { return fail(media.error()); }
            if (!media->empty()) {
                current_printer.capabilities.supported_media.push_back(*media);
            }
        } else if (current_attr_name == "printer-resolution-supported") {
            if (val_len != 9U) { return fail(errc::malformed_data); }
            // Resolution tag: xres (4), yres (4), units (1: 3=dpi, 4=dpcm)
            const auto xres = read_uint32(val_data, val_len, 0U);
            const auto yres = read_uint32(val_data, val_len, 4U);
            const std::uint8_t units = val_data[8];
            if (xres && yres && *xres != 0U && *yres != 0U &&
                *xres <= 0x7FFFFFFFU && *yres <= 0x7FFFFFFFU &&
                (units == 3U || units == 4U)) {
                std::string res_str = std::to_string(*xres) + "x" +
                                      std::to_string(*yres) +
                                      (units == 4 ? " dpcm" : " dpi");
                current_printer.capabilities.supported_resolutions
                    .push_back(std::move(res_str));
            } else { return fail(errc::malformed_data); }
        } else if (current_attr_name == "copies-supported") {
            if (val_len == 4U) {
                const auto max_c = read_uint32(val_data, val_len, 0U);
                if (max_c && *max_c != 0U && *max_c <= 0x7FFFFFFFU) {
                    current_printer.capabilities.max_copies = max_c;
                    current_printer.capabilities.copies = true;
                } else { return fail(errc::malformed_data); }
            } else if (val_len == 8U) {
                // Range: lower (4), upper (4)
                const auto min_c = read_uint32(val_data, val_len, 0U);
                const auto max_c = read_uint32(val_data, val_len, 4U);
                if (min_c && max_c && *min_c != 0U && *min_c <= *max_c &&
                    *max_c <= 0x7FFFFFFFU) {
                    current_printer.capabilities.max_copies = max_c;
                    current_printer.capabilities.copies = true;
                } else { return fail(errc::malformed_data); }
            } else { return fail(errc::malformed_data); }
        } else if (current_attr_name == "sides-supported") {
            const auto sides = validate_utf8(
                std::string_view(reinterpret_cast<const char*>(val_data), val_len));
            if (!sides) { return fail(sides.error()); }
            if (sides->find("two-sided") != std::string_view::npos) {
                current_printer.capabilities.duplex = true;
            }
        }
    }

    if (!saw_end_of_attributes || pos != size) {
        return fail(errc::malformed_data);
    }

    if (in_printer_group) {
        if (!finish_current_printer()) { return fail(errc::malformed_data); }
    }

    return result_printers;
}

} // namespace ipp

/// Parses CUPS `/etc/cups/printers.conf` configuration format.
inline result<std::vector<syscape::printer::printer_info>>
parse_cups_printers_conf(std::string_view content) {
    std::vector<syscape::printer::printer_info> printers;
    syscape::printer::printer_info current;
    bool in_printer = false;
    bool current_is_default = false;
    bool saw_default = false;

    while (!content.empty()) {
        const std::size_t line_end = content.find('\n');
        std::string_view line = (line_end == std::string_view::npos)
                                    ? content
                                    : content.substr(0, line_end);
        content = (line_end == std::string_view::npos)
                      ? std::string_view{}
                      : content.substr(line_end + 1U);

        line = trim_whitespace(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (starts_with_ignore_case(line, "<Printer ") ||
            starts_with_ignore_case(line, "<DefaultPrinter ")) {
            if (in_printer) { return fail(errc::malformed_data); }
            const bool is_def =
                starts_with_ignore_case(line, "<DefaultPrinter ");
            const std::size_t tag_len = is_def ? 16U : 9U;
            const std::size_t close_bracket = line.find('>', tag_len);
            if (close_bracket == std::string_view::npos ||
                !trim_whitespace(line.substr(close_bracket + 1U)).empty()) {
                return fail(errc::malformed_data);
            }
            current = syscape::printer::printer_info{};
            const auto name = validate_utf8(
                trim_whitespace(line.substr(tag_len, close_bracket - tag_len)));
            if (!name) { return fail(name.error()); }
            if (name->empty() || (is_def && saw_default)) {
                return fail(errc::malformed_data);
            }
            current.id = *name;
            current.name = *name;
            current.is_default = is_def;
            current_is_default = is_def;
            saw_default = saw_default || is_def;
            in_printer = true;
            continue;
        }

        if (starts_with_ignore_case(line, "</Printer>") ||
            starts_with_ignore_case(line, "</DefaultPrinter>")) {
            const bool closes_default =
                starts_with_ignore_case(line, "</DefaultPrinter>");
            if (!in_printer || closes_default != current_is_default ||
                !trim_whitespace(line.substr(closes_default ? 17U : 10U)).empty()) {
                return fail(errc::malformed_data);
            }
            current.type = classify_printer_type(current.uri, current.name,
                                                 current.driver_name);
            printers.push_back(current);
            in_printer = false;
            continue;
        }

        if (!in_printer) {
            continue;
        }

        // Parse key-value lines: "Key Value"
        const std::size_t space_pos = line.find(' ');
        if (space_pos == std::string_view::npos) {
            continue;
        }

        const std::string_view key = trim_whitespace(line.substr(0, space_pos));
        const std::string_view val =
            trim_whitespace(line.substr(space_pos + 1U));

        if (equals_ignore_case(key, "Info")) {
            const auto text = validate_utf8(val);
            if (!text) { return fail(text.error()); }
            current.description = *text;
        } else if (equals_ignore_case(key, "Location")) {
            const auto text = validate_utf8(val);
            if (!text) { return fail(text.error()); }
            current.location = *text;
        } else if (equals_ignore_case(key, "MakeModel")) {
            const auto text = validate_utf8(val);
            if (!text) { return fail(text.error()); }
            current.driver_name = *text;
        } else if (equals_ignore_case(key, "DeviceURI")) {
            const auto text = validate_utf8(val);
            if (!text) { return fail(text.error()); }
            current.uri = *text;
        } else if (equals_ignore_case(key, "State")) {
            if (equals_ignore_case(val, "Idle")) {
                current.state = syscape::printer::printer_state::idle;
            } else if (equals_ignore_case(val, "Processing")) {
                current.state = syscape::printer::printer_state::processing;
            } else if (equals_ignore_case(val, "Stopped")) {
                current.state = syscape::printer::printer_state::stopped;
            } else {
                return fail(errc::malformed_data);
            }
        } else if (equals_ignore_case(key, "Accepting")) {
            if (equals_ignore_case(val, "Yes") ||
                equals_ignore_case(val, "True") || val == "1") {
                current.is_accepting_jobs = true;
            } else if (equals_ignore_case(val, "No") ||
                       equals_ignore_case(val, "False") || val == "0") {
                current.is_accepting_jobs = false;
            } else {
                return fail(errc::malformed_data);
            }
        } else if (equals_ignore_case(key, "Shared")) {
            if (equals_ignore_case(val, "Yes") ||
                equals_ignore_case(val, "True") || val == "1") {
                current.is_shared = true;
            } else if (equals_ignore_case(val, "No") ||
                       equals_ignore_case(val, "False") || val == "0") {
                current.is_shared = false;
            } else {
                return fail(errc::malformed_data);
            }
        }
    }

    if (in_printer) { return fail(errc::malformed_data); }

    return printers;
}

} // namespace printer_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_PRINTER_COMMON_HPP
