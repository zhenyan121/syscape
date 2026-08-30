#include <iostream>

#include <syscape/detail/utf8.hpp>
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
    expect(name && !name->empty() && syscape::detail::is_valid_utf8(*name),
           "current user name must be nonempty UTF-8");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
