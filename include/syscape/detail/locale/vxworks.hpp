#ifndef SYSCAPE_DETAIL_LOCALE_VXWORKS_HPP
#define SYSCAPE_DETAIL_LOCALE_VXWORKS_HPP

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <clocale>
#include <ctime>
#include <sys/stat.h>

#include <syscape/detail/locale/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace locale_backend {

inline bool validate_dst_spec(std::string_view spec) noexcept {
    if (spec.size() != 6U) {
        return false;
    }
    for (char c : spec) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    const int month = (spec[0] - '0') * 10 + (spec[1] - '0');
    const int day = (spec[2] - '0') * 10 + (spec[3] - '0');
    const int hour = (spec[4] - '0') * 10 + (spec[5] - '0');

    if (month < 1 || month > 12) {
        return false;
    }
    constexpr int max_days_in_month[] = {0,  31, 29, 31, 30, 31, 30,
                                         31, 31, 30, 31, 30, 31};
    if (day < 1 || day > max_days_in_month[month]) {
        return false;
    }
    if (hour < 0 || hour > 23) {
        return false;
    }
    return true;
}

struct vxworks_timezone_info {
    std::string name;
    std::int32_t offset_seconds = 0;
    bool has_dst = false;
};

inline result<vxworks_timezone_info>
parse_vxworks_timezone(std::string_view text) {
    if (text.empty() || text.find('\0') != std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    if (text == "UTC" || text == "GMT") {
        return vxworks_timezone_info {std::string(text), 0, false};
    }
    std::vector<std::string_view> tokens;
    std::size_t offset = 0U;
    while (offset <= text.size()) {
        const std::size_t next_colon = text.find(':', offset);
        if (next_colon == std::string_view::npos) {
            tokens.push_back(text.substr(offset));
            break;
        }
        tokens.push_back(text.substr(offset, next_colon - offset));
        offset = next_colon + 1U;
    }
    if (tokens.size() != 3U && tokens.size() != 5U) {
        return fail(errc::malformed_data);
    }
    const std::string_view name = tokens[0];
    if (name.empty()) {
        return fail(errc::malformed_data);
    }
    if (!is_valid_utf8(name)) {
        return fail(errc::invalid_encoding);
    }
    for (char c : name) {
        if (static_cast<unsigned char>(c) <= 32 || c == 127 || c == '/') {
            return fail(errc::malformed_data);
        }
    }
    const std::string_view offset_str = tokens[2];
    if (offset_str.empty()) {
        return fail(errc::malformed_data);
    }
    std::size_t idx = 0U;
    bool negative = false;
    if (offset_str[0] == '-') {
        negative = true;
        idx = 1U;
    } else if (offset_str[0] == '+') {
        idx = 1U;
    }
    if (idx >= offset_str.size()) {
        return fail(errc::malformed_data);
    }
    std::int64_t minutes = 0;
    while (idx < offset_str.size()) {
        const char c = offset_str[idx];
        if (c < '0' || c > '9') {
            return fail(errc::malformed_data);
        }
        minutes = minutes * 10 + (c - '0');
        if (minutes > 1440) {
            return fail(errc::malformed_data);
        }
        ++idx;
    }
    if (negative) {
        minutes = -minutes;
    }
    const std::int32_t seconds_east = static_cast<std::int32_t>(-minutes * 60);

    bool has_dst = false;
    if (tokens.size() == 5U) {
        if (!validate_dst_spec(tokens[3]) || !validate_dst_spec(tokens[4])) {
            return fail(errc::malformed_data);
        }
        has_dst = true;
    }

    return vxworks_timezone_info {std::string(name), seconds_east, has_dst};
}

inline result<std::int32_t>
calculate_tm_difference(const std::tm& local, const std::tm& gm) noexcept {
    int days = 0;
    if (local.tm_year == gm.tm_year) {
        days = local.tm_yday - gm.tm_yday;
    } else if (local.tm_year == gm.tm_year + 1) {
        const int gm_year = gm.tm_year + 1900;
        const bool is_leap =
            (gm_year % 4 == 0 && gm_year % 100 != 0) || (gm_year % 400 == 0);
        days = local.tm_yday + (is_leap ? 366 : 365) - gm.tm_yday;
    } else if (local.tm_year == gm.tm_year - 1) {
        const int local_year = local.tm_year + 1900;
        const bool is_leap = (local_year % 4 == 0 && local_year % 100 != 0) ||
                             (local_year % 400 == 0);
        days = local.tm_yday - (is_leap ? 366 : 365) - gm.tm_yday;
    } else {
        return fail(errc::malformed_data);
    }
    const std::int64_t diff =
        static_cast<std::int64_t>(days) * 86400 +
        static_cast<std::int64_t>(local.tm_hour - gm.tm_hour) * 3600 +
        static_cast<std::int64_t>(local.tm_min - gm.tm_min) * 60 +
        static_cast<std::int64_t>(local.tm_sec - gm.tm_sec);
    if (diff <= -86400 || diff >= 86400) {
        return fail(errc::malformed_data);
    }
    return static_cast<std::int32_t>(diff);
}

inline result<std::string> current_locale() {
    const char* const name = std::setlocale(LC_ALL, nullptr);
    if (name == nullptr) {
        return fail(errc::malformed_data);
    }
    std::string result_str(name);
    if (!is_valid_utf8(result_str)) {
        return fail(errc::invalid_encoding);
    }
    return result_str;
}

inline result<std::string> text_encoding() {
    // VxWorks has no <langinfo.h> and no nl_langinfo(CODESET).
    // An honest query returns not_supported when codeset inspection is
    // unavailable.
    return fail(errc::not_supported);
}

inline result<std::int32_t> utc_offset_seconds() {
    const char* const tz_env = std::getenv("TIMEZONE");
    result<vxworks_timezone_info> parsed_tz = fail(errc::not_found);
    if (tz_env != nullptr && tz_env[0] != '\0') {
        parsed_tz = parse_vxworks_timezone(tz_env);
        if (!parsed_tz) {
            return fail(parsed_tz.error());
        }
    }

    ::tzset();
    const std::time_t now = std::time(nullptr);
    if (now == static_cast<std::time_t>(-1)) {
        return fail(std::error_code(EOVERFLOW, std::generic_category()));
    }
    std::tm local {};
    if (::localtime_r(&now, &local) == nullptr) {
        return fail(std::error_code(errno, std::generic_category()));
    }

    std::tm gm {};
    if (::gmtime_r(&now, &gm) != nullptr) {
        auto diff = calculate_tm_difference(local, gm);
        if (diff) {
            return *diff;
        }
        if (parsed_tz && parsed_tz->has_dst) {
            return fail(diff.error());
        }
        if (!parsed_tz) {
            return fail(diff.error());
        }
    } else {
        if (parsed_tz && parsed_tz->has_dst) {
            return fail(errc::not_supported);
        }
        if (!parsed_tz) {
            const int err = errno != 0 ? errno : EINVAL;
            return fail(std::error_code(err, std::generic_category()));
        }
    }

    if (parsed_tz && !parsed_tz->has_dst) {
        return parsed_tz->offset_seconds;
    }

    return fail(errc::not_found);
}

class owned_fd {
    public:
    explicit owned_fd(int fd) noexcept : fd_(fd) {}
    ~owned_fd() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    owned_fd(const owned_fd&) = delete;
    owned_fd& operator=(const owned_fd&) = delete;
    int get() const noexcept {
        return fd_;
    }

    private:
    int fd_;
};

inline result<std::string> validate_zone_identifier(std::string_view value) {
    if (value.empty() || value.front() == '/' || value.back() == '/' ||
        value.find('\0') != std::string_view::npos) {
        return fail(errc::malformed_data);
    }
    if (!is_valid_utf8(value)) {
        return fail(errc::invalid_encoding);
    }
    std::size_t offset = 0U;
    while (offset < value.size()) {
        const std::size_t end = value.find('/', offset);
        const std::string_view part = end == std::string_view::npos
                                          ? value.substr(offset)
                                          : value.substr(offset, end - offset);
        if (part.empty() || part == "." || part == "..") {
            return fail(errc::malformed_data);
        }
        if (end == std::string_view::npos) {
            break;
        }
        offset = end + 1U;
    }
    return std::string(value);
}

inline result<void> validate_zone_file(std::string_view identifier) {
    const std::string path =
        std::string("/usr/share/zoneinfo/") + std::string(identifier);
    int fd = -1;
    for (;;) {
#if defined(O_CLOEXEC)
        fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
#else
        fd = ::open(path.c_str(), O_RDONLY);
#endif
        if (fd >= 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        const int err = errno;
        if (err == ENOENT || err == ENOTDIR) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(err, std::generic_category()));
    }
    const owned_fd guard(fd);
    struct ::stat st {};
    for (;;) {
        if (::fstat(fd, &st) == 0) {
            break;
        }
        if (errno == EINTR) {
            continue;
        }
        const int err = errno;
        return fail(std::error_code(err, std::generic_category()));
    }
    if (!S_ISREG(st.st_mode)) {
        return fail(errc::malformed_data);
    }
    char magic[4];
    std::size_t used = 0U;
    while (used < sizeof(magic)) {
        const ssize_t count = ::read(fd, magic + used, sizeof(magic) - used);
        if (count > 0) {
            used += static_cast<std::size_t>(count);
        } else if (count == 0) {
            return fail(errc::malformed_data);
        } else if (errno == EINTR) {
            continue;
        } else {
            return fail(std::error_code(errno, std::generic_category()));
        }
    }
    if (std::memcmp(magic, "TZif", sizeof(magic)) != 0) {
        return fail(errc::malformed_data);
    }
    return {};
}

inline result<std::vector<std::string>> preferred_languages() {
    return fail(errc::not_supported);
}

inline result<std::string> country_region_code() {
    return fail(errc::not_supported);
}

inline result<std::string> time_zone_identifier() {
    const char* const tz_param = std::getenv("TIMEZONE");
    if (tz_param != nullptr && tz_param[0] != '\0') {
        auto parsed = parse_vxworks_timezone(tz_param);
        if (!parsed) {
            return fail(parsed.error());
        }
        return parsed->name;
    }

    const char* const tz = std::getenv("TZ");
    if (tz != nullptr) {
        std::string_view spec(tz);
        if (spec.empty() || spec == ":") {
            return std::string("UTC");
        }
        if (spec.front() == ':') {
            spec.remove_prefix(1U);
        }
        if (spec == "UTC" || spec == "GMT") {
            return std::string(spec);
        }
        if (spec.find(':') != std::string_view::npos) {
            auto parsed = parse_vxworks_timezone(spec);
            if (parsed) {
                return parsed->name;
            }
        }
        if (spec.find(',') != std::string_view::npos) {
            return fail(errc::not_found);
        }
        auto valid_id = validate_zone_identifier(spec);
        if (!valid_id) {
            return fail(valid_id.error());
        }
        const auto valid_file = validate_zone_file(*valid_id);
        if (!valid_file) {
            return fail(valid_file.error());
        }
        return *valid_id;
    }

    std::size_t size = 256U;
    constexpr std::size_t max_size = 64U * 1024U;
    while (size <= max_size) {
        std::string buf(size, '\0');
        ssize_t len = -1;
        for (;;) {
            errno = 0;
            len = ::readlink("/etc/localtime", &buf[0], size);
            if (len >= 0 || errno != EINTR) {
                break;
            }
        }
        if (len > 0) {
            if (static_cast<std::size_t>(len) < size) {
                buf.resize(static_cast<std::size_t>(len));
                std::string_view target(buf);
                constexpr std::string_view root = "/usr/share/zoneinfo/";
                std::string_view id_view;
                if (target.compare(0U, root.size(), root) == 0) {
                    id_view = target.substr(root.size());
                } else {
                    std::size_t offset = 0U;
                    while (target.substr(offset).compare(0U, 3U, "../") == 0) {
                        offset += 3U;
                    }
                    constexpr std::string_view rel_root = "usr/share/zoneinfo/";
                    if (offset > 0U &&
                        target.substr(offset).compare(0U, rel_root.size(),
                                                      rel_root) == 0) {
                        id_view = target.substr(offset + rel_root.size());
                    }
                }
                if (!id_view.empty()) {
                    auto valid_id = validate_zone_identifier(id_view);
                    if (!valid_id) {
                        return fail(valid_id.error());
                    }
                    const auto valid_file = validate_zone_file(*valid_id);
                    if (!valid_file) {
                        return fail(valid_file.error());
                    }
                    return *valid_id;
                }
                return fail(errc::not_found);
            }
            size *= 2U;
            continue;
        }
        const int err = errno;
        if (err == ENOENT) {
            return fail(errc::not_found);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err != 0 && err != EINVAL) {
            return fail(std::error_code(err, std::generic_category()));
        }
        break;
    }

    return fail(errc::not_found);
}

} // namespace locale_backend
} // namespace detail
} // namespace syscape

#endif
