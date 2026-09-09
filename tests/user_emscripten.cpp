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
    expect(!uid && uid.error() == syscape::errc::not_supported,
           "real_user_id must report not_supported on Emscripten");

    const auto euid = syscape::user::effective_user_id();
    expect(!euid && euid.error() == syscape::errc::not_supported,
           "effective_user_id must report not_supported on Emscripten");

    const auto name = syscape::user::user_name();
    expect(!name && name.error() == syscape::errc::not_supported,
           "user_name must report not_supported on Emscripten");
}

} // namespace

int main() {
    test_user_queries();
    return failures == 0 ? 0 : 1;
}
