#ifndef SYSCAPE_DETAIL_USER_OPENBSD_HPP
#define SYSCAPE_DETAIL_USER_OPENBSD_HPP

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <grp.h>
#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/user/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

template <typename NativeIdentifier>
inline result<std::uint32_t>
narrow_identifier(NativeIdentifier value) noexcept {
    static_assert(std::is_integral<NativeIdentifier>::value,
                  "A POSIX identifier must be an integral type");
    static_assert(std::is_unsigned<NativeIdentifier>::value,
                  "A POSIX identifier must be an unsigned type");
    const std::uintmax_t widened = static_cast<std::uintmax_t>(value);
    if (widened > std::numeric_limits<std::uint32_t>::max()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(widened);
}

inline result<std::uint32_t> real_user_id() {
    return narrow_identifier(::getuid());
}

inline result<std::uint32_t> effective_user_id() {
    return narrow_identifier(::geteuid());
}

inline result<std::uint32_t> real_group_id() {
    return narrow_identifier(::getgid());
}

inline result<std::uint32_t> effective_group_id() {
    return narrow_identifier(::getegid());
}

using passwd_fields = posix_passwd::fields;

inline result<passwd_fields> current_passwd_entry() {
    return posix_passwd::current_entry();
}

inline result<std::string> extract_name(const passwd_fields& fields) {
    if (fields.name.empty()) {
        return fail(errc::malformed_data);
    }
    return fields.name;
}

inline result<std::string> extract_home_directory(const passwd_fields& fields) {
    if (fields.directory.empty() || fields.directory.front() != '/') {
        return fail(errc::malformed_data);
    }
    return fields.directory;
}

inline result<std::string> extract_shell(const passwd_fields& fields) {
    return fields.shell;
}

inline result<std::string> user_name() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) {
        return fail(fields.error());
    }
    return extract_name(*fields);
}

inline result<std::string> home_directory() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) {
        return fail(fields.error());
    }
    return extract_home_directory(*fields);
}

inline result<std::string> shell() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) {
        return fail(fields.error());
    }
    return extract_shell(*fields);
}

template <typename GroupsOperation>
inline result<std::vector<std::uint32_t>>
collect_supplementary_groups(GroupsOperation collect) {
    constexpr std::size_t initial_size = 64U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<::gid_t> recorded(initial_size);

    for (;;) {
        const int fetched =
            collect(static_cast<int>(recorded.size()), recorded.data());
        if (fetched >= 0) {
            recorded.resize(static_cast<std::size_t>(fetched));
            break;
        }
        const int failure_code = errno;
        const bool growable = failure_code == ERANGE || failure_code == EINVAL;
        if (!growable || recorded.size() >= maximum_size) {
            return fail(std::error_code(failure_code, std::generic_category()));
        }
        recorded.resize(recorded.size() <= maximum_size / 2U
                            ? recorded.size() * 2U
                            : maximum_size);
    }

    std::vector<std::uint32_t> narrowed;
    narrowed.reserve(recorded.size());
    for (::gid_t identifier : recorded) {
        result<std::uint32_t> value = narrow_identifier(identifier);
        if (!value) {
            return fail(value.error());
        }
        narrowed.push_back(*value);
    }
    std::sort(narrowed.begin(), narrowed.end());
    narrowed.erase(std::unique(narrowed.begin(), narrowed.end()),
                   narrowed.end());
    return narrowed;
}

inline result<std::vector<std::uint32_t>> supplementary_groups() {
    return collect_supplementary_groups(
        [](int size, ::gid_t* list) { return ::getgroups(size, list); });
}

inline result<user_common::privilege_state> privilege() {
    using syscape::detail::user_common::privilege_state;
    return ::geteuid() == 0 ? privilege_state::privileged
                            : privilege_state::unprivileged;
}

inline std::error_code map_login_failure(int outcome) {
    switch (outcome) {
    case ENXIO:
    case ENOTTY:
    case ENOENT:
        return make_error_code(errc::not_found);
    default:
        return std::error_code(outcome, std::generic_category());
    }
}

template <typename LoginOperation>
inline result<std::string> lookup_login_with_growth(LoginOperation login) {
    constexpr std::size_t initial_size = 256U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        buffer.front() = '\0';
        const int outcome = login(buffer.data(), buffer.size());
        if (outcome == 0) {
            const std::string name(buffer.data());
            if (name.empty()) {
                return fail(errc::malformed_data);
            }
            return name;
        }
        if ((outcome == ERANGE || outcome == EINVAL) &&
            buffer.size() < maximum_size) {
            buffer.resize(buffer.size() <= maximum_size / 2U
                              ? buffer.size() * 2U
                              : maximum_size);
            continue;
        }
        return fail(map_login_failure(outcome));
    }
}

inline result<std::string> login_name() {
    return lookup_login_with_growth([](char* buffer, std::size_t size) {
        return ::getlogin_r(buffer, size);
    });
}

inline result<std::vector<user_common::session_info>> sessions() {
    return fail(errc::not_supported);
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
