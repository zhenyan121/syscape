#ifndef SYSCAPE_DETAIL_PRINTER_LINUX_HPP
#define SYSCAPE_DETAIL_PRINTER_LINUX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <syscape/detail/linux/directory.hpp>
#include <syscape/detail/linux/file.hpp>
#include <syscape/detail/printer/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/printer.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace printer_backend {

/// Queries the local CUPS daemon over Unix domain socket with an IPP request.
inline result<std::vector<std::uint8_t>>
query_cups_socket(const std::vector<std::uint8_t>& ipp_request) {
    static constexpr const char* const socket_paths[] = {
        "/var/run/cups/cups.sock",
        "/run/cups/cups.sock",
        "/var/run/cups.sock"
    };

    int sock = -1;
    for (const char* const path : socket_paths) {
        sock = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
        if (sock < 0) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        struct sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

        struct timeval tv{};
        tv.tv_sec = 1;
        tv.tv_usec = 0;
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

    const linux_platform::file_descriptor sock_fd(sock);

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
        const ssize_t n = ::send(
            sock, send_buf.data() + sent, send_buf.size() - sent, MSG_NOSIGNAL);
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
        const ssize_t n = ::recv(sock, chunk, sizeof(chunk), 0);
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

/// Discovers raw USB printer devices under /sys/class/usbmisc.
inline result<std::vector<printer::printer_info>> discover_sysfs_usb_printers() {
    std::vector<printer::printer_info> found;
    const linux_platform::directory_handle handle("/sys/class/usbmisc");
    if (!handle.valid()) {
        if (handle.error() == ENOENT || handle.error() == ENOTDIR) {
            return found;
        }
        return fail(std::error_code(handle.error(), std::generic_category()));
    }

    for (;;) {
        errno = 0;
        const ::dirent* const entry = ::readdir(handle.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        const std::string_view name(entry->d_name);
        if (name.size() <= 2U || name.rfind("lp", 0) != 0 ||
            !std::all_of(name.begin() + 2, name.end(), [](char value) {
                return value >= '0' && value <= '9';
            })) {
            continue;
        }

        printer::printer_info p;
        p.id = std::string(name);
        p.name = std::string(name);
        p.type = printer::printer_type::local;
        const std::string device_node = "/dev/usb/" + std::string(name);
        struct stat node_status{};
        if (::stat(device_node.c_str(), &node_status) == 0) {
            if (S_ISCHR(node_status.st_mode)) { p.uri = device_node; }
        } else if (errno != ENOENT && errno != ENOTDIR) {
            return fail(std::error_code(errno, std::generic_category()));
        }

        // Try reading manufacturer/product from USB device parent
        const std::string dev_path = "/sys/class/usbmisc/" + std::string(name) + "/device";
        const auto vendor = linux_platform::read_text_file((dev_path + "/../manufacturer").c_str());
        const auto product = linux_platform::read_text_file((dev_path + "/../product").c_str());
        if (!vendor && vendor.error() != std::errc::no_such_file_or_directory) {
            return fail(vendor.error());
        }
        if (!product && product.error() != std::errc::no_such_file_or_directory) {
            return fail(product.error());
        }
        std::string full_name;
        if (vendor) {
            full_name = *vendor;
            linux_platform::trim_line_end(full_name);
            const auto valid_vendor = printer_common::validate_utf8(full_name);
            if (!valid_vendor) { return fail(valid_vendor.error()); }
            full_name = *valid_vendor;
        }
        if (product) {
            std::string prod_str = *product;
            linux_platform::trim_line_end(prod_str);
            const auto valid_product = printer_common::validate_utf8(prod_str);
            if (!valid_product) { return fail(valid_product.error()); }
            prod_str = *valid_product;
            if (!full_name.empty() && !prod_str.empty()) {
                full_name += " ";
            }
            full_name += prod_str;
        }
        if (!full_name.empty()) {
            p.name = full_name;
            p.driver_name = full_name;
        }

        found.push_back(std::move(p));
    }

    return found;
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

inline void merge_usb_printers(std::vector<printer::printer_info>& list,
                               std::vector<printer::printer_info> usb_list) {
    for (auto& usb : usb_list) {
        const std::string usb_identity =
            printer_common::normalized_printer_identity(usb.name);
        const bool duplicate = std::any_of(
            list.begin(), list.end(), [&](const printer::printer_info& item) {
                const bool matching_identity = !usb_identity.empty() &&
                    (printer_common::normalized_printer_identity(item.id) ==
                         usb_identity ||
                     printer_common::normalized_printer_identity(item.name) ==
                         usb_identity ||
                     (printer_common::starts_with_ignore_case(item.uri, "usb://") &&
                      printer_common::normalized_printer_identity(item.uri) ==
                          usb_identity));
                return item.id == usb.id ||
                       (!usb.uri.empty() && item.uri == usb.uri) ||
                       ((printer_common::starts_with_ignore_case(item.uri, "usb://") ||
                         item.type == printer::printer_type::local) &&
                        (matching_identity ||
                         (!usb.name.empty() &&
                          printer_common::equals_ignore_case(item.name, usb.name)) ||
                         (!usb.driver_name.empty() &&
                         printer_common::equals_ignore_case(item.driver_name,
                                                            usb.driver_name))));
            });
        if (!duplicate) { list.push_back(std::move(usb)); }
    }
}

struct printer_snapshot {
    std::vector<printer::printer_info> items;
    std::optional<std::error_code> default_error;
    bool default_known = false;
    std::string default_name;
};

/// Takes one printer snapshot while keeping default-query failure independent
/// from a successfully enumerated printer list.
inline result<printer_snapshot> query_printer_snapshot() {
    const auto environment_default = environment_default_printer();
    if (!environment_default) { return fail(environment_default.error()); }

    printer_snapshot snapshot;
    if (*environment_default) {
        snapshot.default_known = true;
        snapshot.default_name = **environment_default;
    }
    bool have_authoritative_list = false;

    // 1. Try querying CUPS daemon via local Unix socket.
    const auto req = printer_common::ipp::build_get_printers_request();
    const auto sock_res = query_cups_socket(req);
    if (sock_res) {
        const auto parse_res =
            printer_common::ipp::parse_ipp_printers_response(
                sock_res->data(), sock_res->size());
        if (!parse_res) { return fail(parse_res.error()); }
        snapshot.items = *parse_res;
        have_authoritative_list = true;

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
    } else if (sock_res.error() != errc::not_supported) {
        return fail(sock_res.error());
    }

    // 2. Fall back to the persistent CUPS configuration only when no daemon
    // was available.
    if (!have_authoritative_list) {
        const auto conf_text =
            linux_platform::read_text_file("/etc/cups/printers.conf");
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
        } else if (conf_text.error() != std::errc::no_such_file_or_directory) {
            return fail(conf_text.error());
        }
    }

    // 3. Add directly discoverable USB printer devices without inventing
    // queue state for them.
    auto usb_list = discover_sysfs_usb_printers();
    if (!usb_list) { return fail(usb_list.error()); }
    merge_usb_printers(snapshot.items, std::move(*usb_list));

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

/// Enumerates all installed printers on Linux.
inline result<std::vector<printer::printer_info>> printers() {
    const auto snapshot = query_printer_snapshot();
    if (!snapshot) { return fail(snapshot.error()); }
    return snapshot->items;
}

/// Returns the count of installed printers.
inline result<std::size_t> printer_count() {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }
    return list->size();
}

/// Returns the system default printer.
inline result<printer::printer_info> default_printer() {
    const auto snapshot = query_printer_snapshot();
    if (!snapshot) {
        return fail(snapshot.error());
    }
    return printer_common::resolve_snapshot_default(
        snapshot->items, snapshot->default_known, snapshot->default_error);
}

/// Finds a printer by queue name or display name.
inline result<printer::printer_info> find_printer(std::string_view name_or_id) {
    const auto list = printers();
    if (!list) {
        return fail(list.error());
    }

    // Exact match first
    for (const auto& item : *list) {
        if (item.id == name_or_id || item.name == name_or_id) {
            return item;
        }
    }

    // Case-insensitive match
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

#endif // SYSCAPE_DETAIL_PRINTER_LINUX_HPP
