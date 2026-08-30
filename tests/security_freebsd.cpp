#include <iostream>

#include <syscape/security.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_security_queries() {
    const auto aslr_res = syscape::security::aslr();
    expect(aslr_res.has_value() || aslr_res.error() == syscape::errc::not_found,
           "aslr query must succeed or report not_found");
}

} // namespace

int main() {
    test_security_queries();
    return failures == 0 ? 0 : 1;
}
