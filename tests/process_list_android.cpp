#include <iostream>
#include <limits>

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
    expect(procs.has_value() ||
               procs.error() == syscape::errc::permission_denied,
           "process list query must succeed or report permission_denied");

    const auto count = syscape::process_list::process_count();
    expect(count.has_value() ||
               count.error() == syscape::errc::permission_denied,
           "process_count query must succeed or report permission_denied");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
