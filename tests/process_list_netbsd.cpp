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
    expect(procs && !procs->empty(),
           "process list must return at least one entry");

    const auto count = syscape::process_list::process_count();
    expect(count && *count > 0, "process_count must be positive");

    const auto oversized = syscape::process_list::find_process(
        (std::numeric_limits<std::uint32_t>::max)());
    expect(!oversized && oversized.error() == syscape::errc::value_too_large,
           "an unrepresentable native PID must report value_too_large");
}

} // namespace

int main() {
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
