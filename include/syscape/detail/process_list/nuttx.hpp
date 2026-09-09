#ifndef SYSCAPE_DETAIL_PROCESS_LIST_NUTTX_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_NUTTX_HPP

#if defined(__NuttX__) &&                                                      \
    (!defined(CONFIG_FS_PROCFS) || defined(CONFIG_FS_PROCFS_EXCLUDE_PROCESS))
#include <syscape/detail/process_list/generic.hpp>
#else

#include <cerrno>
#include <cstdint>
#include <dirent.h>
#include <fcntl.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <utility>
#include <vector>

#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

class nuttx_dir_guard {
    public:
    explicit nuttx_dir_guard(::DIR* value) noexcept : value_(value) {}
    nuttx_dir_guard(const nuttx_dir_guard&) = delete;
    nuttx_dir_guard& operator=(const nuttx_dir_guard&) = delete;
    ~nuttx_dir_guard() {
        if (value_ != nullptr) {
            static_cast<void>(::closedir(value_));
        }
    }
    ::DIR* get() const noexcept {
        return value_;
    }

    private:
    ::DIR* value_;
};

inline bool nuttx_parse_pid(const char* text, std::uint32_t& result) noexcept {
    if (text == nullptr || text[0] == '\0') {
        return false;
    }
    std::uint32_t value = 0U;
    for (const char* cursor = text; *cursor != '\0'; ++cursor) {
        if (*cursor < '0' || *cursor > '9') {
            return false;
        }
        const auto digit = static_cast<std::uint32_t>(*cursor - '0');
        if (value >
            ((std::numeric_limits<std::uint32_t>::max)() - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
    }
    result = value;
    return true;
}

inline result<void> nuttx_parse_status(std::string_view content,
                                       process_list::process_entry& entry) {
    while (!content.empty()) {
        const std::size_t newline = content.find('\n');
        std::string_view line = newline == std::string_view::npos
                                    ? content
                                    : content.substr(0U, newline);
        content = newline == std::string_view::npos
                      ? std::string_view()
                      : content.substr(newline + 1U);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos) {
            continue;
        }
        std::string_view value = line.substr(colon + 1U);
        while (!value.empty() &&
               (value.front() == ' ' || value.front() == '\t')) {
            value.remove_prefix(1U);
        }
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                                  value.back() == '\r')) {
            value.remove_suffix(1U);
        }
        const std::string_view key = line.substr(0U, colon);
        if (key == "Name") {
            if (value.empty()) {
                return fail(errc::malformed_data);
            }
            if (!is_valid_utf8(value)) {
                return fail(errc::invalid_encoding);
            }
            entry.name = std::string(value);
        } else if (key == "State") {
            if (value.rfind("Running", 0U) == 0U ||
                value.rfind("Ready", 0U) == 0U) {
                entry.state = process_list::process_state::running;
            } else if (value.rfind("Waiting", 0U) == 0U ||
                       value.rfind("Inactive", 0U) == 0U) {
                entry.state = process_list::process_state::sleeping;
            }
        }
    }
    return {};
}

inline result<bool> nuttx_read_status(std::uint32_t pid,
                                      process_list::process_entry& entry) {
    const std::string path = "/proc/" + std::to_string(pid) + "/status";
    errno = 0;
    const int descriptor = ::open(path.c_str(), O_RDONLY);
    if (descriptor < 0) {
        const int error = errno;
        if (error == ENOENT || error == ESRCH) {
            return false;
        }
        if (error == EACCES || error == EPERM) {
            return true;
        }
        return fail(std::error_code(error, std::generic_category()));
    }
    struct fd_guard {
        int value;
        ~fd_guard() {
            static_cast<void>(::close(value));
        }
    } guard {descriptor};

    std::string content;
    char buffer[512];
    constexpr std::size_t maximum = 16U * 1024U;
    for (;;) {
        const ssize_t count = ::read(descriptor, buffer, sizeof(buffer));
        if (count > 0) {
            const auto amount = static_cast<std::size_t>(count);
            if (amount > maximum - content.size()) {
                return fail(errc::value_too_large);
            }
            content.append(buffer, amount);
        } else if (count == 0) {
            break;
        } else if (errno != EINTR) {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    const auto parsed = nuttx_parse_status(content, entry);
    return parsed ? result<bool>(true) : fail(parsed.error());
}

inline result<std::vector<process_list::process_entry>> processes() {
    ::DIR* raw = ::opendir("/proc");
    if (raw == nullptr) {
        const int error = errno;
        if (error == ENOENT || error == ENOTDIR) {
            return fail(errc::not_supported);
        }
        if (error == EACCES || error == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(error, std::generic_category()));
    }
    nuttx_dir_guard directory(raw);
    std::vector<process_list::process_entry> result;
    for (;;) {
        errno = 0;
        struct ::dirent* item = ::readdir(directory.get());
        if (item == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        std::uint32_t pid = 0U;
        if (!nuttx_parse_pid(item->d_name, pid)) {
            continue;
        }
        process_list::process_entry entry;
        entry.pid = pid;
        const auto observed = nuttx_read_status(pid, entry);
        if (!observed) {
            return fail(observed.error());
        }
        if (*observed) {
            result.push_back(std::move(entry));
        }
    }
    process_list_common::sort_processes(result);
    return result;
}

inline result<std::uint32_t> process_count() {
    const auto value = processes();
    if (!value) {
        return fail(value.error());
    }
    if (value->size() > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(value->size());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    process_list::process_entry entry;
    entry.pid = pid;
    const auto observed = nuttx_read_status(pid, entry);
    if (!observed) {
        return fail(observed.error());
    }
    return *observed ? result<process_list::process_entry>(std::move(entry))
                     : fail(errc::not_found);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    if (name.empty()) {
        return fail(errc::invalid_argument);
    }
    const auto value = processes();
    if (!value) {
        return fail(value.error());
    }
    std::vector<process_list::process_entry> matches;
    for (const auto& entry : *value) {
        if (process_list_common::matches_process_name(entry, name, false)) {
            matches.push_back(entry);
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
#endif
