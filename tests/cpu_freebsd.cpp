#include <iostream>
#include <string>

#include <syscape/cpu.hpp>
#include <syscape/detail/utf8.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpu_queries() {
    const auto count = syscape::cpu::logical_core_count();
    expect(count && *count > 0, "logical core count must be positive");

    const auto physical = syscape::cpu::physical_core_count();
    expect(physical && *physical > 0, "physical core count must be positive");

    const auto model = syscape::cpu::model_name();
    expect(!model ||
               (!model->empty() && syscape::detail::is_valid_utf8(*model)),
           "model name must be valid UTF-8 if present");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
