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

    const auto models = syscape::cpu::model_names();
    expect(models || models.error() == syscape::errc::not_found ||
               models.error() == syscape::errc::not_supported,
           "model names must succeed or report an unavailable-source error");
    if (models) {
        for (const auto& model : *models) {
            expect(!model.empty(), "model names must be nonempty");
        }
    }

    const auto min_f = syscape::cpu::minimum_frequency_khz();
    expect(min_f.has_value() ||
               min_f.error() == syscape::errc::permission_denied ||
               min_f.error() == syscape::errc::not_supported,
           "minimum frequency must succeed, report permission_denied, or "
           "not_supported");

    const auto max_f = syscape::cpu::maximum_frequency_khz();
    expect(max_f.has_value() ||
               max_f.error() == syscape::errc::permission_denied ||
               max_f.error() == syscape::errc::not_supported,
           "maximum frequency must succeed, report permission_denied, or "
           "not_supported");
}

} // namespace

int main() {
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
