#include <iostream>

#include <syscape/virtualization.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_virtualization_queries() {
    const auto hyp = syscape::virtualization::hypervisor();
    expect(!hyp && hyp.error() == syscape::errc::not_supported,
           "hypervisor must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_virtualization_queries();
    return failures == 0 ? 0 : 1;
}
