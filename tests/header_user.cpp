#include <cstdint>
#include <string>

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
    const syscape::result<std::string> name = syscape::user::user_name();
    const syscape::result<std::string> home =
        syscape::user::home_directory();
    const syscape::result<std::string> shell = syscape::user::shell();

    static_cast<void>(real_user);
    static_cast<void>(effective_user);
    static_cast<void>(real_group);
    static_cast<void>(effective_group);
    static_cast<void>(home);
    static_cast<void>(shell);
    return name && name->empty() ? 1 : 0;
}
