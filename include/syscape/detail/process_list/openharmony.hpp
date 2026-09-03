#ifndef SYSCAPE_DETAIL_PROCESS_LIST_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_PROCESS_LIST_OPENHARMONY_HPP

#include <algorithm>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <dirent.h>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/openharmony/directory.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/process_list/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/process_list.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace process_list_backend {

inline result<process_list::process_entry>
make_entry_from_pid(std::uint32_t pid) {
    const std::string pid_dir = "/proc/" + std::to_string(pid);
    const auto stat_content =
        openharmony::read_text_file((pid_dir + "/stat").c_str());
    if (!stat_content) {
        return fail(stat_content.error());
    }

    const std::size_t name_start = stat_content->find('(');
    const std::size_t name_end = stat_content->rfind(')');
    if (name_start == std::string_view::npos ||
        name_end == std::string_view::npos || name_end < name_start) {
        return fail(errc::malformed_data);
    }

    process_list::process_entry entry {};
    entry.pid = pid;
    entry.name =
        stat_content->substr(name_start + 1U, name_end - name_start - 1U);
    if (entry.name && !detail::is_valid_utf8(*entry.name)) {
        return fail(errc::invalid_encoding);
    }

    const auto cmdline =
        openharmony::read_text_file((pid_dir + "/cmdline").c_str());
    if (cmdline) {
        if (!cmdline->empty()) {
            std::string_view sv = *cmdline;
            if (sv.back() != '\0') {
                return fail(errc::malformed_data);
            }
            sv.remove_suffix(1U);
            std::vector<std::string> args;
            std::size_t start = 0U;
            for (;;) {
                const std::size_t end = sv.find('\0', start);
                if (end == std::string_view::npos) {
                    std::string_view arg = sv.substr(start);
                    if (!detail::is_valid_utf8(arg)) {
                        return fail(errc::invalid_encoding);
                    }
                    args.emplace_back(arg);
                    break;
                }
                std::string_view arg = sv.substr(start, end - start);
                if (!detail::is_valid_utf8(arg)) {
                    return fail(errc::invalid_encoding);
                }
                args.emplace_back(arg);
                start = end + 1U;
            }
            entry.command_line = std::move(args);
        }
    } else if (cmdline.error() != errc::not_found &&
               cmdline.error() != errc::permission_denied) {
        return fail(cmdline.error());
    }

    return entry;
}

inline result<std::vector<process_list::process_entry>> processes() {
    openharmony::directory_handle dir("/proc");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<process_list::process_entry> list;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') {
            continue;
        }

        std::uint32_t pid = 0U;
        std::string_view name_sv(entry->d_name);
        const auto r = std::from_chars(name_sv.data(),
                                       name_sv.data() + name_sv.size(), pid);
        if (r.ec != std::errc() || r.ptr != name_sv.data() + name_sv.size() ||
            pid == 0U) {
            continue;
        }

        auto pentry = make_entry_from_pid(pid);
        if (pentry) {
            list.push_back(std::move(*pentry));
        } else if (pentry.error() != errc::not_found &&
                   pentry.error() != errc::permission_denied) {
            return fail(pentry.error());
        }
    }

    std::sort(list.begin(), list.end(),
              [](const process_list::process_entry& a,
                 const process_list::process_entry& b) noexcept {
                  return a.pid < b.pid;
              });

    return list;
}

inline result<std::uint32_t> process_count() {
    const auto all = processes();
    if (!all) {
        return fail(all.error());
    }
    return static_cast<std::uint32_t>(all->size());
}

inline result<process_list::process_entry> find_process(std::uint32_t pid) {
    if (pid == 0U) {
        return fail(errc::not_found);
    }
    return make_entry_from_pid(pid);
}

inline result<std::vector<process_list::process_entry>>
find_processes_by_name(std::string_view name) {
    const auto all = processes();
    if (!all) {
        return fail(all.error());
    }
    std::vector<process_list::process_entry> matches;
    for (auto& proc : *all) {
        if (proc.name == name) {
            matches.push_back(std::move(proc));
        }
    }
    return matches;
}

} // namespace process_list_backend
} // namespace detail
} // namespace syscape

#endif
