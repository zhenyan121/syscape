#ifndef SYSCAPE_DETAIL_POSIX_PASSWD_HPP
#define SYSCAPE_DETAIL_POSIX_PASSWD_HPP

#include <cerrno>
#include <cstddef>
#include <string>
#include <system_error>
#include <vector>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace posix_passwd {

/// Textual identity fields copied from a passwd database entry.
struct fields {
    std::string name;
    std::string directory;
    std::string shell;
};

/// Performs a reentrant passwd lookup with bounded buffer growth.
template <typename LookupOperation>
inline result<fields> lookup_with_growth(LookupOperation lookup) {
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

        fields value;
        value.name = pointer->pw_name != nullptr
                         ? std::string(pointer->pw_name)
                         : std::string();
        value.directory = pointer->pw_dir != nullptr
                              ? std::string(pointer->pw_dir)
                              : std::string();
        value.shell = pointer->pw_shell != nullptr
                          ? std::string(pointer->pw_shell)
                          : std::string();
        return value;
    }
}

/// Looks up the passwd entry for the calling process's effective user.
inline result<fields> current_entry() {
    const ::uid_t uid = ::geteuid();
    return lookup_with_growth(
        [uid](::passwd& entry, char* buffer, std::size_t size,
              ::passwd** pointer) {
            return ::getpwuid_r(uid, &entry, buffer, size, pointer);
        });
}

} // namespace posix_passwd
} // namespace detail
} // namespace syscape

#endif
