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
    const auto s = syscape::software::services();
    expect(!s && s.error() == syscape::errc::not_supported,
           "services query must report not_supported on Haiku");
}

} // namespace

int main() {
    test_software_queries();
    return failures == 0 ? 0 : 1;
}
