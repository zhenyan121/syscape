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
    expect((name && !name->empty()) || name.error() == syscape::errc::not_found,
           "user name must be nonempty or not_found");

    const auto home = syscape::user::home_directory();
    expect((home && !home->empty() && home->front() == '/') ||
               home.error() == syscape::errc::not_found,
           "home directory must be an absolute path or not_found");

    const auto priv = syscape::user::privilege();
    expect(priv.has_value(), "privilege query must succeed");

    const auto groups = syscape::user::supplementary_groups();
    expect(groups.has_value(), "supplementary groups query must succeed");

    const auto sessions = syscape::user::sessions();
    expect(sessions.has_value() ||
               sessions.error() == syscape::errc::not_supported,
           "sessions query must succeed or report not_supported");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
