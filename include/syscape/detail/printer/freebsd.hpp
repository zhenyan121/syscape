#ifndef SYSCAPE_DETAIL_PRINTER_FREEBSD_HPP
#define SYSCAPE_DETAIL_PRINTER_FREEBSD_HPP

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
        "/var/run/cups/cups.sock", "/var/run/cups.sock"};

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
        if (::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0 ||
            ::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            const int error = errno;
            ::close(sock);
            return fail(std::error_code(error, std::generic_category()));
        }

#ifdef SO_NOSIGPIPE
        int nosigpipe = 1;
        static_cast<void>(::setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE,
                                       &nosigpipe, sizeof(nosigpipe)));
#endif

        if (::connect(sock, reinterpret_cast<struct sockaddr*>(&addr),
                      sizeof(addr)) == 0) {
            break;
        }
        ::close(sock);
        sock = -1;
    }

    if (sock < 0) {
        return fail(errc::not_supported);
    }
    const freebsd_file_descriptor owned_socket(sock);
    static_cast<void>(owned_socket);

    std::string http_header = "POST / HTTP/1.1\r\n"
                              "Host: localhost\r\n"
                              "Content-Type: application/ipp\r\n"
                              "Content-Length: " +
                              std::to_string(ipp_request.size()) +
                              "\r\n"
                              "Connection: close\r\n"
                              "\r\n";

    std::vector<std::uint8_t> payload;
    payload.reserve(http_header.size() + ipp_request.size());
    payload.insert(payload.end(), http_header.begin(), http_header.end());
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
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EPIPE || errno == ECONNRESET) {
                return fail(errc::temporarily_unavailable);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (sent == 0) {
            return fail(errc::io_error);
        }
        written += static_cast<std::size_t>(sent);
    }

    std::vector<std::uint8_t> response;
    std::uint8_t buffer[4096];
    for (;;) {
        const ssize_t received = ::recv(sock, buffer, sizeof(buffer), 0);
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return fail(errc::temporarily_unavailable);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (received == 0) {
            break;
        }
        if (response.size() + static_cast<std::size_t>(received) >
            16U * 1024U * 1024U) {
            return fail(errc::value_too_large);
        }
        response.insert(response.end(), buffer, buffer + received);
    }

    return response;
}

inline result<std::vector<printer_common::parsed_printer>>
parse_freebsd_printers_conf() {
    static constexpr const char* const config_paths[] = {
        "/usr/local/etc/cups/printers.conf", "/etc/cups/printers.conf"};
    for (const char* path : config_paths) {
        const result<std::string> content = read_text_file(path);
        if (content) {
            return printer_common::parse_printers_conf_content(*content);
        }
    }
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::printer::printer_info>>
discover_freebsd_printers() {
    const std::vector<std::uint8_t> req =
        printer_common::encode_ipp_get_printers_request(1);
    const result<std::vector<std::uint8_t>> resp = query_cups_socket(req);

    std::vector<printer_common::parsed_printer> parsed;
    if (resp) {
        std::vector<std::uint8_t> body;
        const result<void> extract_res =
            printer_common::extract_http_body(*resp, body);
        if (extract_res) {
            const result<std::vector<printer_common::parsed_printer>> ipp_res =
                printer_common::parse_ipp_response(body);
            if (ipp_res) {
                parsed = std::move(*ipp_res);
            }
        }
    }

    if (parsed.empty()) {
        const result<std::vector<printer_common::parsed_printer>> conf_res =
            parse_freebsd_printers_conf();
        if (conf_res) {
            parsed = std::move(*conf_res);
        } else if (!resp) {
            return fail(resp.error());
        }
    }

    std::string default_queue;
    const char* const lpdest = ::getenv("LPDEST");
    const char* const prn = ::getenv("PRINTER");
    if (lpdest != nullptr && lpdest[0] != '\0') {
        default_queue = lpdest;
    } else if (prn != nullptr && prn[0] != '\0') {
        default_queue = prn;
    }

    std::vector<::syscape::printer::printer_info> list;
    list.reserve(parsed.size());
    for (auto& p : parsed) {
        if (!default_queue.empty() && p.info.id == default_queue) {
            p.info.is_default = true;
        }
        list.push_back(std::move(p.info));
    }

    std::sort(list.begin(), list.end(),
              [](const ::syscape::printer::printer_info& a,
                 const ::syscape::printer::printer_info& b) noexcept {
                  return a.id < b.id;
              });

    return list;
}

inline result<std::vector<::syscape::printer::printer_info>> printers() {
    return discover_freebsd_printers();
}

inline result<std::size_t> printer_count() {
    const result<std::vector<::syscape::printer::printer_info>> list =
        printers();
    if (!list) {
        return fail(list.error());
    }
    return list->size();
}

inline result<::syscape::printer::printer_info> default_printer() {
    const result<std::vector<::syscape::printer::printer_info>> list =
        printers();
    if (!list) {
        return fail(list.error());
    }
    for (const auto& p : *list) {
        if (p.is_default && *p.is_default) {
            return p;
        }
    }
    return fail(errc::not_found);
}

inline result<::syscape::printer::printer_info>
find_printer(std::string_view name_or_id) {
    const result<std::vector<::syscape::printer::printer_info>> list =
        printers();
    if (!list) {
        return fail(list.error());
    }
    for (const auto& p : *list) {
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
