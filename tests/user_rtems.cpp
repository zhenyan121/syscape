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
    expect(uid.has_value(), "real user id query must succeed");

    const auto euid = syscape::user::effective_user_id();
    expect(euid.has_value(), "effective user id query must succeed");

    const auto gid = syscape::user::real_group_id();
    expect(gid.has_value(), "real group id query must succeed");

    const auto egid = syscape::user::effective_group_id();
    expect(egid.has_value(), "effective group id query must succeed");

    const auto name = syscape::user::user_name();
    expect(!name && name.error() == syscape::errc::not_supported,
           "user name must report not_supported on RTEMS");

    const auto home = syscape::user::home_directory();
    expect(!home && home.error() == syscape::errc::not_supported,
           "home directory must report not_supported on RTEMS");

    const auto shell = syscape::user::shell();
    expect(!shell && shell.error() == syscape::errc::not_supported,
           "shell must report not_supported on RTEMS");

    const auto priv = syscape::user::privilege();
    expect(priv.has_value(), "privilege query must succeed");

    const auto groups = syscape::user::supplementary_groups();
    expect(!groups && groups.error() == syscape::errc::not_supported,
           "supplementary groups must report not_supported on RTEMS");

    const auto login = syscape::user::login_name();
    expect(!login && login.error() == syscape::errc::not_supported,
           "login name must report not_supported on RTEMS");

    const auto sessions = syscape::user::sessions();
    expect(sessions.error() == syscape::errc::not_supported,
           "sessions query must report not_supported on RTEMS");

    const auto logged_in = syscape::user::logged_in_users();
    expect(logged_in.error() == syscape::errc::not_supported,
           "logged-in users query must report not_supported on RTEMS");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
