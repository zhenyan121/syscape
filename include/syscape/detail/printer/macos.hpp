#ifndef SYSCAPE_DETAIL_PRINTER_MACOS_HPP
#define SYSCAPE_DETAIL_PRINTER_MACOS_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/printer/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/printer.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_backend {

class macos_file_descriptor {
public:
    explicit macos_file_descriptor(int value) noexcept : value_(value) {}
    macos_file_descriptor(const macos_file_descriptor&) = delete;
    macos_file_descriptor& operator=(const macos_file_descriptor&) = delete;
    ~macos_file_descriptor() {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

private:
    int value_;
};

inline result<std::string> read_text_file(
    const char* path, std::size_t maximum_size = 1024U * 1024U) {
    const int descriptor = ::open(path, O_RDONLY);
    if (descriptor < 0) {
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) {
        const int error = errno;
        ::close(descriptor);
        return fail(std::error_code(error, std::generic_category()));
    }
    const macos_file_descriptor owned_descriptor(descriptor);
    static_cast<void>(owned_descriptor);

    char buffer[4096];
    std::string output;
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            const std::size_t size = static_cast<std::size_t>(count);
            if (output.size() > maximum_size ||
                size > maximum_size - output.size()) {
                return fail(errc::value_too_large);
            }
            output.append(buffer, size);
            continue;
        }
        if (count == 0) {
            return output;
        }
        if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
}

inline result<std::vector<std::uint8_t>>
query_cups_socket(const std::vector<std::uint8_t>& ipp_request) {
    static constexpr const char* const socket_paths[] = {
        "/private/var/run/cupsd",
        "/var/run/cups/cups.sock",
        "/run/cups/cups.sock",
        "/var/run/cups.sock"
    };

    int sock = -1;
    for (const char* const path : socket_paths) {
        sock = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (sock < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (::fcntl(sock, F_SETFD, FD_CLOEXEC) != 0) {
            const int error = errno;
            ::close(sock);
            return fail(std::error_code(error, std::generic_category()));
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
        addr.sun_len = static_cast<std::uint8_t>(SUN_LEN(&addr));

        struct timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        const int no_sigpipe = 1;
        if (::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                         sizeof(no_sigpipe)) != 0 ||
            ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
            ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            const int error = errno;
            ::close(sock);
            return fail(std::error_code(error, std::generic_category()));
        }

        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                      SUN_LEN(&addr)) == 0) {
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

    const macos_file_descriptor sock_fd(sock);

    std::string http_req =
        "POST / HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "User-Agent: syscape\r\n"
        "Connection: close\r\n"
        "Content-Type: application/ipp\r\n"
        "Content-Length: " +
        std::to_string(ipp_request.size()) + "\r\n\r\n";

    std::vector<std::uint8_t> send_buf;
    send_buf.reserve(http_req.size() + ipp_request.size());
    for (char c : http_req) {
        send_buf.push_back(static_cast<std::uint8_t>(c));
    }
    send_buf.insert(send_buf.end(), ipp_request.begin(), ipp_request.end());

    std::size_t sent = 0;
    while (sent < send_buf.size()) {
        const ssize_t n = ::write(
            sock, send_buf.data() + sent, send_buf.size() - sent);
        if (n > 0) {
            sent += static_cast<std::size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return fail(errc::temporarily_unavailable);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::vector<std::uint8_t> recv_buf;
    std::uint8_t chunk[4096];
    for (;;) {
        const ssize_t n = ::read(sock, chunk, sizeof(chunk));
        if (n > 0) {
            recv_buf.insert(recv_buf.end(), chunk, chunk + n);
            if (recv_buf.size() > std::size_t{16U} * 1024U * 1024U) {
                return fail(errc::value_too_large);
            }
            continue;
        }
        if (n == 0) {
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

    if (recv_buf.empty()) {
        return fail(errc::malformed_data);
    }
    return printer_common::parse_http_ipp_response(recv_buf);
}

inline result<std::optional<std::string>> environment_default_printer() {
    const char* value = std::getenv("LPDEST");
    if (value == nullptr || *value == '\0') {
        value = std::getenv("PRINTER");
    }
    if (value == nullptr || *value == '\0') {
        return std::optional<std::string>{};
    }
    const auto validated = printer_common::validate_utf8(value);
    if (!validated) { return fail(validated.error()); }
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
    if (!environment_default) { return fail(environment_default.error()); }

    printer_snapshot snapshot;
    if (*environment_default) {
        snapshot.default_known = true;
        snapshot.default_name = **environment_default;
    }

    // 1. Try querying CUPS socket.
    const auto req = printer_common::ipp::build_get_printers_request();
    const auto sock_res = query_cups_socket(req);
    if (sock_res) {
        const auto parse_res =
            printer_common::ipp::parse_ipp_printers_response(
                sock_res->data(), sock_res->size());
        if (!parse_res) { return fail(parse_res.error()); }
        snapshot.items = *parse_res;
        if (!*environment_default) {
            const auto def_req = printer_common::ipp::build_get_default_request();
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

    // 2. Fall back to the persistent configuration only when no daemon exists.
    const auto conf_text = read_text_file("/etc/cups/printers.conf");
    if (conf_text) {
        const auto parsed = printer_common::parse_cups_printers_conf(*conf_text);
        if (!parsed) { return fail(parsed.error()); }
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

    return snapshot;
}

inline result<std::vector<printer::printer_info>> printers() {
    const auto snapshot = query_printer_snapshot();
    if (!snapshot) { return fail(snapshot.error()); }
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

#endif // SYSCAPE_DETAIL_PRINTER_MACOS_HPP
