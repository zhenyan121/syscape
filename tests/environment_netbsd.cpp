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
    const auto cwd = syscape::environment::current_working_directory();
    expect(cwd && !cwd->empty(), "current working directory must be nonempty");

    const auto temp = syscape::environment::temp_directory();
    expect(temp && !temp->empty(), "temporary directory must be nonempty");
}

} // namespace

int main() {
    test_environment_queries();
    return failures == 0 ? 0 : 1;
}
