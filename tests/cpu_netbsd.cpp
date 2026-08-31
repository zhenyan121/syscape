#include <iostream>
#include <string>

#include <syscape/cpu.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpu_queries() {
    const auto count = syscape::cpu::online_logical_processor_count();
    expect(count && *count > 0, "logical core count must be positive");

    const auto physical = syscape::cpu::online_physical_core_count();
    expect(!physical && physical.error() == syscape::errc::not_supported,
           "physical core count must report not_supported on NetBSD");

    const auto models = syscape::cpu::model_names();
    expect(models || models.error() == syscape::errc::not_found ||
               models.error() == syscape::errc::not_supported,
           "model names must succeed or report an unavailable-source error");
    if (models) {
        for (const auto& model : *models) {
            expect(!model.empty(), "model names must be nonempty");
        }
    }
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
