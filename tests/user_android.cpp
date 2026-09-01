#include <iostream>

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
    const auto login = syscape::user::login_name();
    expect((login && !login->empty()) ||
               login.error() == syscape::errc::not_found ||
               login.error() == syscape::errc::not_supported,
           "login name must be nonempty, report not_found, or report "
           "not_supported");

    const auto name = syscape::user::user_name();
    expect(name.has_value() || name.error() == syscape::errc::not_found ||
               name.error() == syscape::errc::not_supported,
           "user name must succeed, report not_found, or report not_supported");

    const auto home = syscape::user::home_directory();
    expect(home.has_value() || home.error() == syscape::errc::not_found ||
               home.error() == syscape::errc::not_supported,
           "home directory must succeed, report not_found, or report "
           "not_supported");

    const auto priv = syscape::user::privilege();
    expect(priv.has_value(), "privilege query must succeed");

    const auto sess = syscape::user::sessions();
    expect(sess.has_value() || sess.error() == syscape::errc::not_supported,
           "sessions query must succeed or report not_supported");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
