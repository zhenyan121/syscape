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
    expect(procs && !procs->empty(),
           "processes list must not be empty on FreeBSD");

    const auto count = syscape::process_list::process_count();
    expect(count && *count > 0, "process_count must be positive");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
