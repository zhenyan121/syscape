#include <iostream>

#include <syscape/software.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_software_queries() {
    const auto svcs = syscape::software::services();
    expect(svcs.has_value(), "services query must succeed");

    const auto drvs = syscape::software::loaded_drivers();
    expect(drvs.has_value(), "loaded_drivers query must succeed");
}

} // namespace

int main() {
    test_software_queries();
    return failures == 0 ? 0 : 1;
}
