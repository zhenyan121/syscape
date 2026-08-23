#include <cstdint>
#include <string>
#include <vector>

#include <syscape/user.hpp>
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
    const syscape::result<std::vector<std::uint32_t>> groups =
        syscape::user::supplementary_groups();
    const syscape::result<syscape::user::privilege_state> privilege =
        syscape::user::privilege();
    const syscape::result<std::string> name = syscape::user::user_name();
    const syscape::result<std::string> session = syscape::user::login_name();
    const syscape::result<std::string> home =
        syscape::user::home_directory();
    const syscape::result<std::string> shell = syscape::user::shell();

    static_cast<void>(real_user);
    static_cast<void>(effective_user);
    static_cast<void>(real_group);
    static_cast<void>(effective_group);
    static_cast<void>(groups);
    static_cast<void>(privilege);
    static_cast<void>(session);
    static_cast<void>(home);
    static_cast<void>(shell);
    return name && name->empty() ? 1 : 0;
}
