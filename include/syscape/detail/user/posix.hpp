#ifndef SYSCAPE_DETAIL_USER_POSIX_HPP
#define SYSCAPE_DETAIL_USER_POSIX_HPP

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
struct passwd_fields {
    std::string name;
    std::string directory;
    std::string shell;
};

template <typename LookupOperation>
inline result<passwd_fields> lookup_passwd_with_growth(
    LookupOperation lookup) {
    constexpr std::size_t initial_size = 1024U;
    constexpr std::size_t maximum_size = 1024U * 1024U;
    std::vector<char> buffer(initial_size);

    for (;;) {
        ::passwd entry {};
        ::passwd* pointer = nullptr;
        const int outcome =
            lookup(entry, buffer.data(), buffer.size(), &pointer);
        if (outcome != 0) {
            if (outcome == ERANGE && buffer.size() < maximum_size) {
                buffer.resize(buffer.size() <= maximum_size / 2U
                                  ? buffer.size() * 2U
                                  : maximum_size);
                continue;
            }
            return fail(std::error_code(outcome, std::generic_category()));
        }
        if (pointer == nullptr) {
            return fail(errc::not_found);
        }

        // Copy every present field verbatim before the buffer disappears.
        // Field-level validation belongs to the individual queries so that
        // one unusable field cannot invalidate unrelated information.
        passwd_fields fields;
        fields.name =
            pointer->pw_name != nullptr ? std::string(pointer->pw_name)
                                        : std::string();
        fields.directory =
            pointer->pw_dir != nullptr ? std::string(pointer->pw_dir)
                                       : std::string();
        fields.shell =
            pointer->pw_shell != nullptr ? std::string(pointer->pw_shell)
                                         : std::string();
        return fields;
    }
}

/// Looks up the passwd entry of the effective user identifier.
///
/// Each call performs a fresh lookup so that changes to the user database
/// become visible without caching.
inline result<passwd_fields> current_passwd_entry() {
    const ::uid_t uid = ::geteuid();
    return lookup_passwd_with_growth(
        [uid](::passwd& entry, char* buffer, std::size_t size,
              ::passwd** pointer) {
            return ::getpwuid_r(uid, &entry, buffer, size, pointer);
        });
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

} // namespace user_backend
} // namespace detail
} // namespace syscape

#endif
