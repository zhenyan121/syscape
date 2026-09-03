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
    const auto uid = syscape::user::real_user_id();
    expect(uid.has_value(), "real user id query must succeed");
    const auto euid = syscape::user::effective_user_id();
    expect(euid.has_value(), "effective user id query must succeed");
    const auto uname = syscape::user::user_name();
    expect(uname && !uname->empty(), "user name must be nonempty");
    const auto home = syscape::user::home_directory();
    expect(home && !home->empty(), "home directory must be nonempty");
    const auto sess = syscape::user::sessions();
    expect(!sess && sess.error() == syscape::errc::not_supported,
           "sessions query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
