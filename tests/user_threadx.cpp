#include <iostream>
#include <string>

#include <syscape/user.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_user_queries() {
    const auto uid = syscape::user::real_user_id();
    expect(!uid && uid.error() == syscape::errc::not_supported,
           "real user id query must report not_supported on ThreadX");

    const auto euid = syscape::user::effective_user_id();
    expect(!euid && euid.error() == syscape::errc::not_supported,
           "effective user id query must report not_supported on ThreadX");

    const auto gid = syscape::user::real_group_id();
    expect(!gid && gid.error() == syscape::errc::not_supported,
           "real group id query must report not_supported on ThreadX");

    const auto egid = syscape::user::effective_group_id();
    expect(!egid && egid.error() == syscape::errc::not_supported,
           "effective group id query must report not_supported on ThreadX");

    const auto name = syscape::user::user_name();
    expect(!name && name.error() == syscape::errc::not_supported,
           "user name must report not_supported on ThreadX");

    const auto home = syscape::user::home_directory();
    expect(!home && home.error() == syscape::errc::not_supported,
           "home directory must report not_supported on ThreadX");

    const auto shell = syscape::user::shell();
    expect(!shell && shell.error() == syscape::errc::not_supported,
           "shell must report not_supported on ThreadX");

    const auto priv = syscape::user::privilege();
    expect(!priv && priv.error() == syscape::errc::not_supported,
           "privilege query must report not_supported on ThreadX");

    const auto groups = syscape::user::supplementary_groups();
    expect(!groups && groups.error() == syscape::errc::not_supported,
           "supplementary groups must report not_supported on ThreadX");

    const auto login = syscape::user::login_name();
    expect(!login && login.error() == syscape::errc::not_supported,
           "login name must report not_supported on ThreadX");

    const auto sessions = syscape::user::sessions();
    expect(!sessions && sessions.error() == syscape::errc::not_supported,
           "sessions query must report not_supported on ThreadX");

    const auto logged_in = syscape::user::logged_in_users();
    expect(!logged_in && logged_in.error() == syscape::errc::not_supported,
           "logged-in users query must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
