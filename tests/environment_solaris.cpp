#include <iostream>
#include <string>

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
    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd && !cwd->empty() && cwd->front() == '/',
           "current working directory must be an absolute path");

    const auto tmp = syscape::environment::temp_directory();
    expect(tmp && !tmp->empty() && tmp->front() == '/',
           "temp directory must be an absolute path");

    const auto home = syscape::environment::home_directory();
    expect(home.has_value(), "home directory query must succeed");

    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value(), "environment variables query must succeed");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
