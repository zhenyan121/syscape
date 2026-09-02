#include <iostream>

#include <syscape/environment.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_environment_queries() {
    const auto vars = syscape::environment::variables();
    expect(vars.has_value(), "environment variables query must succeed");

    const auto tmp = syscape::environment::temp_directory();
    expect(tmp && !tmp->empty(), "temp directory must be nonempty");

    const auto home = syscape::environment::home_directory();
    expect(home.has_value() || home.error() == syscape::errc::not_found,
           "home directory query must succeed or report not_found");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
