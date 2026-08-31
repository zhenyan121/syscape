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
    const auto name = syscape::user::user_name();
    expect(name && !name->empty(), "user name must be nonempty");

    const auto home = syscape::user::home_directory();
    expect(home && !home->empty(), "home directory must be nonempty");

    const auto priv = syscape::user::privilege();
    expect(priv.has_value(), "privilege query must succeed");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
