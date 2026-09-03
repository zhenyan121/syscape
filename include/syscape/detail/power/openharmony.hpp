#ifndef SYSCAPE_DETAIL_POWER_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_POWER_OPENHARMONY_HPP

#include <cerrno>
#include <charconv>
#include <cstdint>
#include <dirent.h>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include <syscape/detail/openharmony/directory.hpp>
#include <syscape/detail/openharmony/file.hpp>
#include <syscape/detail/power/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_backend {

inline result<power_common::battery_condition>
parse_condition(std::string_view input) {
    std::string_view val = input;
    openharmony::strip_trailing_newlines(val);
    using battery_condition = power_common::battery_condition;
    if (val == "Charging") {
        return battery_condition::charging;
    }
    if (val == "Discharging") {
        return battery_condition::discharging;
    }
    if (val == "Not charging") {
        return battery_condition::not_charging;
    }
    if (val == "Full") {
        return battery_condition::full;
    }
    return battery_condition::unknown;
}

inline result<std::vector<power_common::battery_record>> batteries() {
    openharmony::directory_handle dir("/sys/class/power_supply");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        if (dir.error() == ENOENT) {
            return std::vector<power_common::battery_record> {};
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    std::vector<power_common::battery_record> list;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string dir_path =
            std::string("/sys/class/power_supply/") + entry->d_name;
        const std::string type_path = dir_path + "/type";
        const auto type_content =
            openharmony::read_text_file(type_path.c_str());
        if (!type_content) {
            if (type_content.error() == errc::not_found) {
                continue;
            }
            return fail(type_content.error());
        }
        std::string_view type_sv = *type_content;
        openharmony::strip_trailing_newlines(type_sv);
        if (type_sv != "Battery") {
            continue;
        }

        power_common::battery_record bat {};
        bat.identifier = entry->d_name;

        bool is_present = true;
        const auto pres_val =
            openharmony::read_text_file((dir_path + "/present").c_str());
        if (pres_val) {
            std::string_view psv = *pres_val;
            openharmony::strip_trailing_newlines(psv);
            if (psv == "1") {
                is_present = true;
            } else if (psv == "0") {
                is_present = false;
            } else {
                return fail(errc::malformed_data);
            }
        } else if (pres_val.error() != errc::not_found) {
            return fail(pres_val.error());
        }
        bat.present = is_present;

        const auto status_val =
            openharmony::read_text_file((dir_path + "/status").c_str());
        if (status_val) {
            auto cond = parse_condition(*status_val);
            if (cond) {
                bat.condition = *cond;
            }
        } else if (status_val.error() != errc::not_found) {
            return fail(status_val.error());
        }

        const auto cap_val =
            openharmony::read_text_file((dir_path + "/capacity").c_str());
        if (cap_val) {
            std::uint32_t pct = 0U;
            std::string_view trimmed = *cap_val;
            openharmony::strip_trailing_newlines(trimmed);
            auto r = std::from_chars(trimmed.data(),
                                     trimmed.data() + trimmed.size(), pct);
            if (r.ec != std::errc() ||
                r.ptr != trimmed.data() + trimmed.size() || pct > 100U) {
                return fail(errc::malformed_data);
            }
            bat.has_charge_percent = true;
            bat.charge_percent = pct;
        } else if (cap_val.error() != errc::not_found) {
            return fail(cap_val.error());
        }

        list.push_back(std::move(bat));
    }
    return list;
}

inline result<power_common::external_presence> external_power_online() {
    openharmony::directory_handle dir("/sys/class/power_supply");
    if (!dir.valid()) {
        if (dir.error() == EACCES || dir.error() == EPERM) {
            return fail(errc::permission_denied);
        }
        if (dir.error() == ENOENT) {
            return power_common::external_presence::no_evidence;
        }
        return fail(std::error_code(dir.error(), std::generic_category()));
    }

    bool found_source = false;
    for (;;) {
        errno = 0;
        struct dirent* entry = ::readdir(dir.get());
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::generic_category()));
            }
            break;
        }
        if (entry->d_name[0] == '.') {
            continue;
        }
        const std::string dir_path =
            std::string("/sys/class/power_supply/") + entry->d_name;
        const std::string type_path = dir_path + "/type";
        const auto type_content =
            openharmony::read_text_file(type_path.c_str());
        if (!type_content) {
            if (type_content.error() == errc::not_found) {
                continue;
            }
            return fail(type_content.error());
        }
        std::string_view type = *type_content;
        openharmony::strip_trailing_newlines(type);
        if (type == "Mains" || type == "USB" || type == "Wireless") {
            const auto online_content =
                openharmony::read_text_file((dir_path + "/online").c_str());
            if (online_content) {
                std::string_view onl = *online_content;
                openharmony::strip_trailing_newlines(onl);
                if (onl == "1") {
                    return power_common::external_presence::connected;
                }
                if (onl != "0") {
                    return fail(errc::malformed_data);
                }
                found_source = true;
            } else if (online_content.error() != errc::not_found) {
                return fail(online_content.error());
            }
        }
    }
    return found_source ? power_common::external_presence::disconnected
                        : power_common::external_presence::no_evidence;
}

inline result<std::vector<power_common::power_source_record>> power_sources() {
    return fail(errc::not_supported);
}

inline result<std::uint64_t> seconds_until_empty() {
    return fail(errc::not_supported);
}

} // namespace power_backend
} // namespace detail
} // namespace syscape

#endif
