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
    expect(uid.has_value(), "real user ID query must succeed");

    const auto gid = syscape::user::real_group_id();
    expect(gid.has_value(), "real group ID query must succeed");

    const auto name = syscape::user::user_name();
    expect((name && !name->empty()) ||
               name.error() == syscape::errc::not_found ||
               name.error() == syscape::errc::permission_denied,
           "user name must be nonempty or report expected error");

    const auto home = syscape::user::home_directory();
    expect((home && !home->empty()) ||
               home.error() == syscape::errc::not_found ||
               home.error() == syscape::errc::permission_denied,
           "home directory must be nonempty or report expected error");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
