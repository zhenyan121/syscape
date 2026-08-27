#include <cstdint>
#include <string>
#include <system_error>

#include <sys/types.h>
#include <unistd.h>

#include <syscape/detail/utf8.hpp>
#include <syscape/user.hpp>

int main() {
    const syscape::result<std::uint32_t> real_user =
        syscape::user::real_user_id();
    const syscape::result<std::uint32_t> effective_user =
        syscape::user::effective_user_id();
    const syscape::result<std::uint32_t> real_group =
        syscape::user::real_group_id();
    const syscape::result<std::uint32_t> effective_group =
        syscape::user::effective_group_id();

    if (!real_user || *real_user != static_cast<std::uint32_t>(::getuid())) {
        return 1;
    }
    if (!effective_user ||
        *effective_user != static_cast<std::uint32_t>(::geteuid())) {
        return 2;
    }
    if (!real_group || *real_group != static_cast<std::uint32_t>(::getgid())) {
        return 3;
    }
    if (!effective_group ||
        *effective_group != static_cast<std::uint32_t>(::getegid())) {
        return 4;
    }

    const syscape::result<std::string> name = syscape::user::user_name();
    if (!name || name->empty() || !syscape::detail::is_valid_utf8(*name)) {
        return 5;
    }

    const syscape::result<std::string> home =
        syscape::user::home_directory();
    if (!home || home->empty() || home->front() != '/' ||
        !syscape::detail::is_valid_utf8(*home)) {
        return 6;
    }

    const syscape::result<std::string> shell = syscape::user::shell();
    if (!shell || !syscape::detail::is_valid_utf8(*shell)) { return 7; }

    const auto sessions = syscape::user::sessions();
    if (!sessions) { return 8; }

    const auto logged_in = syscape::user::logged_in_users();
    if (!logged_in) { return 9; }

    return 0;
}
