#ifndef SYSCAPE_DETAIL_USER_POSIX_HPP
#define SYSCAPE_DETAIL_USER_POSIX_HPP

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>

#include <syscape/detail/posix/passwd.hpp>
#include <syscape/detail/user/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace user_backend {

/// Narrows a POSIX user or group identifier to the portable width.
template <typename NativeIdentifier>
inline result<std::uint32_t> narrow_identifier(
    NativeIdentifier value) noexcept {
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

/// Returns the calling process's real user identifier.
inline result<std::uint32_t> real_user_id() {
    return narrow_identifier(::getuid());
}

/// Returns the calling process's effective user identifier.
inline result<std::uint32_t> effective_user_id() {
    return narrow_identifier(::geteuid());
}

/// Returns the calling process's real group identifier.
inline result<std::uint32_t> real_group_id() {
    return narrow_identifier(::getgid());
}

/// Returns the calling process's effective group identifier.
inline result<std::uint32_t> effective_group_id() {
    return narrow_identifier(::getegid());
}

/// Textual identity fields copied from a passwd database entry.
using passwd_fields = posix_passwd::fields;

template <typename LookupOperation>
inline result<passwd_fields> lookup_passwd_with_growth(
    LookupOperation lookup) {
    return posix_passwd::lookup_with_growth(lookup);
}

/// Looks up the passwd entry of the effective user identifier.
///
/// Each call performs a fresh lookup so that changes to the user database
/// become visible without caching.
inline result<passwd_fields> current_passwd_entry() {
    return posix_passwd::current_entry();
}

/// Extracts the login name from an looked-up passwd entry.
///
/// An empty recorded name carries no usable identity, so it is malformed
/// platform data here; encoding validation remains at the public boundary.
inline result<std::string> extract_name(const passwd_fields& fields) {
    if (fields.name.empty()) { return fail(errc::malformed_data); }
    return fields.name;
}

/// Extracts the recorded home directory from a looked-up passwd entry.
///
/// The passwd database records absolute home directories, so an empty or
/// relative directory is malformed platform data.
inline result<std::string> extract_home_directory(
    const passwd_fields& fields) {
    if (fields.directory.empty() || fields.directory.front() != '/') {
        return fail(errc::malformed_data);
    }
    return fields.directory;
}

/// Extracts the recorded shell from a looked-up passwd entry.
///
/// The shell is reported verbatim; an empty recording is valid data meaning
/// that the platform records no shell for the account.
inline result<std::string> extract_shell(const passwd_fields& fields) {
    return fields.shell;
}

inline result<std::string> user_name() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) { return fail(fields.error()); }
    return extract_name(*fields);
}

inline result<std::string> home_directory() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) { return fail(fields.error()); }
    return extract_home_directory(*fields);
}

inline result<std::string> shell() {
    const result<passwd_fields> fields = current_passwd_entry();
    if (!fields) { return fail(fields.error()); }
    return extract_shell(*fields);
}

/// Collects the supplementary group identifiers through an injectable
/// operation with the POSIX getgroups signature.
///
/// The recorded membership can change between the sizing and retrieval
/// attempts, so the collection retries into a larger buffer while the
/// platform reports an insufficient one, bounded by a generous cap that no
/// documented platform approaches. The portable contract reports ascending
/// unique identifiers because platforms do not document ordering or
/// uniqueness of the recorded set; an empty recording is valid data meaning
/// that the process belongs to no supplementary group beyond its effective
/// group.
template <typename GroupsOperation>
inline result<std::vector<std::uint32_t>> collect_supplementary_groups(
    GroupsOperation collect) {
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
        const bool growable =
            failure_code == ERANGE || failure_code == EINVAL;
        if (!growable || recorded.size() >= maximum_size) {
            return fail(std::error_code(failure_code,
                                        std::generic_category()));
        }
        recorded.resize(recorded.size() <= maximum_size / 2U
                            ? recorded.size() * 2U
                            : maximum_size);
    }

    std::vector<std::uint32_t> narrowed;
    narrowed.reserve(recorded.size());
    for (::gid_t identifier : recorded) {
        result<std::uint32_t> value = narrow_identifier(identifier);
        if (!value) { return fail(value.error()); }
        narrowed.push_back(*value);
    }
    std::sort(narrowed.begin(), narrowed.end());
    narrowed.erase(std::unique(narrowed.begin(), narrowed.end()),
                   narrowed.end());
    return narrowed;
}

/// Returns the supplementary group identifiers of the calling process.
inline result<std::vector<std::uint32_t>> supplementary_groups() {
    return collect_supplementary_groups(
        [](int size, ::gid_t* list) { return ::getgroups(size, list); });
}

/// Classifies the privilege of the effective identity.
///
/// POSIX platforms define user identifier zero as the privileged account, so
/// an effective identifier of zero classifies as privileged. The
/// classification cannot fail on these platforms.
inline result<user_common::privilege_state> privilege() {
    using syscape::detail::user_common::privilege_state;
    return ::geteuid() == 0 ? privilege_state::privileged
                            : privilege_state::unprivileged;
}

/// Maps a getlogin_r failure to the portable error model.
///
/// A process without a controlling terminal, or whose session has no
/// recorded entry, carries no login-session identity. That condition is data
/// absence rather than a system failure, so the documented conditions report
/// not_found while every other condition preserves its native error code.
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

/// Looks up the login name of the controlling-terminal session through an
/// injectable operation with the POSIX getlogin_r signature.
///
/// Each call performs a fresh lookup so session changes become visible
/// between calls. An empty recording is malformed platform data rather than
/// valid information; encoding validation remains at the public boundary.
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
            if (name.empty()) { return fail(errc::malformed_data); }
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

/// Returns the login name recorded for the calling process's session.
inline result<std::string> login_name() {
    return lookup_login_with_growth(
        [](char* buffer, std::size_t size) {
            return ::getlogin_r(buffer, size);
        });
}

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
