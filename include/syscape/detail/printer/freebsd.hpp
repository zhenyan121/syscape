#ifndef SYSCAPE_DETAIL_PRINTER_FREEBSD_HPP
#define SYSCAPE_DETAIL_PRINTER_FREEBSD_HPP

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
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
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

class freebsd_file_descriptor {
    public:
    explicit freebsd_file_descriptor(int value) noexcept : value_(value) {}
    freebsd_file_descriptor(const freebsd_file_descriptor&) = delete;
    freebsd_file_descriptor& operator=(const freebsd_file_descriptor&) = delete;
    ~freebsd_file_descriptor() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    private:
    int value_;
};

inline result<std::string>
read_text_file(const char* path, std::size_t maximum_size = 1024U * 1024U) {
    const int descriptor = ::open(path, O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    const freebsd_file_descriptor owned_descriptor(descriptor);
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

inline result<std::vector<std::uint8_t>>
query_cups_socket(const std::vector<std::uint8_t>& ipp_request) {
    static constexpr const char* const socket_paths[] = {
        "/var/run/cups/cups.sock", "/var/run/cups.sock", "/run/cups/cups.sock"};

    int sock = -1;
    for (const char* const path : socket_paths) {
        sock = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        struct sockaddr_un addr {};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        struct timeval tv {};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
#ifdef SO_NOSIGPIPE
        const int no_sigpipe = 1;
        static_cast<void>(::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE,
                                       &no_sigpipe, sizeof(no_sigpipe)));
#endif
        if (::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
            ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            const int error = errno;
            ::close(sock);
            return fail(std::error_code(error, std::generic_category()));
        }

        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) == 0) {
            break;
        }

        const int error = errno;
        ::close(sock);
        sock = -1;
        if (error != ENOENT && error != ECONNREFUSED) {
            return fail(std::error_code(error, std::generic_category()));
        }
    }

    if (sock < 0) {
        return fail(errc::not_supported);
    }
    const freebsd_file_descriptor owned_socket(sock);
    static_cast<void>(owned_socket);

    std::string http_header = "POST / HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "User-Agent: syscape\r\n"
                              "Content-Type: application/ipp\r\n"
                              "Content-Length: " +
                              std::to_string(ipp_request.size()) +
                              "\r\n"
                              "Connection: close\r\n"
                              "\r\n";

    std::vector<std::uint8_t> payload;
    payload.reserve(http_header.size() + ipp_request.size());
    for (char c : http_header) {
        payload.push_back(static_cast<std::uint8_t>(c));
    }
    payload.insert(payload.end(), ipp_request.begin(), ipp_request.end());

#ifdef MSG_NOSIGNAL
    constexpr int send_flags = MSG_NOSIGNAL;
#else
    constexpr int send_flags = 0;
#endif

    std::size_t written = 0U;
    while (written < payload.size()) {
        const ssize_t sent = ::send(sock, payload.data() + written,
                                    payload.size() - written, send_flags);
        if (sent > 0) {
            written += static_cast<std::size_t>(sent);
            continue;
        }
        if (sent < 0 && errno == EINTR) {
            continue;
        }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return fail(errc::temporarily_unavailable);
        }
        if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
            return fail(errc::temporarily_unavailable);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<std::uint8_t> response;
    std::uint8_t buffer[4096];
    for (;;) {
        const ssize_t received = ::recv(sock, buffer, sizeof(buffer), 0);
        if (received > 0) {
            response.insert(response.end(), buffer, buffer + received);
            if (response.size() > 16U * 1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            continue;
        }
        if (received == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return fail(errc::temporarily_unavailable);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    if (response.empty()) {
        return fail(errc::malformed_data);
    }
    return printer_common::parse_http_ipp_response(response);
}

inline result<std::optional<std::string>> environment_default_printer() {
    const char* value = std::getenv("LPDEST");
    if (value == nullptr || *value == '\0') {
        value = std::getenv("PRINTER");
    }
    if (value == nullptr || *value == '\0') {
        return std::optional<std::string> {};
    }
    const auto validated = printer_common::validate_utf8(value);
    if (!validated) {
        return fail(validated.error());
    }
    return std::optional<std::string>(*validated);
}

struct printer_snapshot {
    std::vector<printer::printer_info> items;
    std::optional<std::error_code> default_error;
    bool default_known = false;
    std::string default_name;
};

inline result<printer_snapshot> query_printer_snapshot() {
    const auto environment_default = environment_default_printer();
    if (!environment_default) {
        return fail(environment_default.error());
    }

    printer_snapshot snapshot;
    if (*environment_default) {
        snapshot.default_known = true;
        snapshot.default_name = **environment_default;
    }

    // 1. Try querying CUPS socket
    const auto req = printer_common::ipp::build_get_printers_request();
    const auto sock_res = query_cups_socket(req);
    if (sock_res) {
        const auto parse_res = printer_common::ipp::parse_ipp_printers_response(
            sock_res->data(), sock_res->size());
        if (!parse_res) {
            return fail(parse_res.error());
        }
        snapshot.items = *parse_res;
        if (!*environment_default) {
            const auto def_req =
                printer_common::ipp::build_get_default_request();
            const auto def_sock_res = query_cups_socket(def_req);
            if (!def_sock_res) {
                snapshot.default_error =
                    def_sock_res.error() == errc::not_supported
                        ? make_error_code(errc::temporarily_unavailable)
                        : def_sock_res.error();
            } else {
                const auto def_parse =
                    printer_common::ipp::parse_ipp_printers_response(
                        def_sock_res->data(), def_sock_res->size());
                if (!def_parse) {
                    if (def_parse.error() == errc::not_found) {
                        snapshot.default_known = true;
                    } else {
                        snapshot.default_error = def_parse.error();
                    }
                } else {
                    snapshot.default_known = true;
                    if (!def_parse->empty()) {
                        snapshot.default_name = (*def_parse)[0].name;
                    }
                }
            }
        }
        if (snapshot.default_known) {
            printer_common::set_default_by_name(snapshot.items,
                                                snapshot.default_name);
        }
        std::sort(snapshot.items.begin(), snapshot.items.end(),
                  [](const auto& a, const auto& b) {
                      return printer_common::natural_less(a.name, b.name);
                  });
        return snapshot;
    }
    if (sock_res.error() != errc::not_supported) {
        return fail(sock_res.error());
    }

    // 2. Fall back to configuration file
    static constexpr const char* const config_paths[] = {
        "/usr/local/etc/cups/printers.conf", "/etc/cups/printers.conf"};
    for (const char* conf_path : config_paths) {
        const auto conf_text = read_text_file(conf_path);
        if (conf_text) {
            const auto parsed =
                printer_common::parse_cups_printers_conf(*conf_text);
            if (!parsed) {
                return fail(parsed.error());
            }
            snapshot.items = *parsed;
            if (!*environment_default) {
                snapshot.default_known = true;
                for (const auto& item : snapshot.items) {
                    if (item.is_default.value_or(false)) {
                        snapshot.default_name = item.name;
                        break;
                    }
                }
            }
            if (snapshot.default_known) {
                printer_common::set_default_by_name(snapshot.items,
                                                    snapshot.default_name);
            }
            std::sort(snapshot.items.begin(), snapshot.items.end(),
                      [](const auto& a, const auto& b) {
                          return printer_common::natural_less(a.name, b.name);
                      });
            return snapshot;
        }
        if (conf_text.error() != std::errc::no_such_file_or_directory) {
            return fail(conf_text.error());
        }
    }

    return snapshot;
}

inline result<std::vector<printer::printer_info>> printers() {
    const auto snapshot = query_printer_snapshot();
    if (!snapshot) {
        return fail(snapshot.error());
    }
    return snapshot->items;
}

inline result<std::size_t> printer_count() {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }
    return list->size();
}

inline result<printer::printer_info> default_printer() {
    const auto snapshot = query_printer_snapshot();
    if (!snapshot) {
        return fail(snapshot.error());
    }
    return printer_common::resolve_snapshot_default(
        snapshot->items, snapshot->default_known, snapshot->default_error);
}

inline result<printer::printer_info> find_printer(std::string_view name_or_id) {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }

    for (const auto& item : *list) {
        if (item.id == name_or_id || item.name == name_or_id) {
            return item;
        }
    }

    for (const auto& item : *list) {
        if (printer_common::equals_ignore_case(item.id, name_or_id) ||
            printer_common::equals_ignore_case(item.name, name_or_id)) {
            return item;
        }
    }

    return fail(errc::not_found);
}

} // namespace printer_backend
} // namespace detail
} // namespace syscape

#endif
