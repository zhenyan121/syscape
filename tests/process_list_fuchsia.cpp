#include <iostream>

#include <syscape/process_list.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_process_list_queries() {
    const auto procs = syscape::process_list::processes();
    expect(!procs && procs.error() == syscape::errc::not_supported,
           "processes must report not_supported on Fuchsia");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
