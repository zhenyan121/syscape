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
    const auto vars = syscape::environment::environment_variables();
    expect(vars.has_value(), "environment variables query must succeed");
    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd && !cwd->empty(), "current working directory must be nonempty");
    const auto tmp = syscape::environment::temp_directory();
    expect(tmp && !tmp->empty(), "temp directory must be nonempty");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
