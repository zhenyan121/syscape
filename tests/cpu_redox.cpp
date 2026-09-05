#include <iostream>
#include <string>
#include <vector>

#include <syscape/cpu.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpu_parsing() {
    const auto c4 = syscape::detail::cpu_backend::parse_cpu_count("CPUs: 4\n");
    expect(c4.has_value() && *c4 == 4U, "parse_cpu_count must parse 4 CPUs");

    const auto c1 = syscape::detail::cpu_backend::parse_cpu_count("CPUs: 1");
    expect(c1.has_value() && *c1 == 1U, "parse_cpu_count must parse 1 CPU");

    const auto c_ws =
        syscape::detail::cpu_backend::parse_cpu_count("CPUs:   16  \n");
    expect(c_ws.has_value() && *c_ws == 16U,
           "parse_cpu_count must parse with whitespace");

    const auto c_zero =
        syscape::detail::cpu_backend::parse_cpu_count("CPUs: 0\n");
    expect(!c_zero && c_zero.error() == syscape::errc::malformed_data,
           "parse_cpu_count must reject zero CPUs");

    const auto c_bad =
        syscape::detail::cpu_backend::parse_cpu_count("CPUs: invalid\n");
    expect(!c_bad && c_bad.error() == syscape::errc::malformed_data,
           "parse_cpu_count must reject non-numeric");

    const auto c_empty =
        syscape::detail::cpu_backend::parse_cpu_count("other: info\n");
    expect(!c_empty && c_empty.error() == syscape::errc::malformed_data,
           "parse_cpu_count must reject missing CPUs line");
}

struct fake_cpu_reader {
    static std::string mock_data;
    static syscape::result<std::string> read_cpu_file() {
        if (mock_data.empty()) {
            return syscape::fail(syscape::errc::not_supported);
        }
        return mock_data;
    }
};

std::string fake_cpu_reader::mock_data;

void test_injected_cpu() {
    fake_cpu_reader::mock_data = "CPUs: 8\n";
    const auto res =
        syscape::detail::cpu_backend::online_logical_processor_count<
            fake_cpu_reader>();
    expect(res.has_value() && *res == 8U, "injected CPU reader must return 8");

    fake_cpu_reader::mock_data = "invalid data";
    const auto bad =
        syscape::detail::cpu_backend::online_logical_processor_count<
            fake_cpu_reader>();
    expect(!bad && bad.error() == syscape::errc::malformed_data,
           "injected invalid CPU must fail with malformed_data");

    fake_cpu_reader::mock_data.clear();
    const auto missing =
        syscape::detail::cpu_backend::online_logical_processor_count<
            fake_cpu_reader>();
    expect(!missing && missing.error() == syscape::errc::not_supported,
           "missing CPU scheme must report not_supported");
}

void test_cpu_queries() {
    const auto count = syscape::cpu::online_logical_processor_count();
    expect(count.error() == syscape::errc::not_supported,
           "logical core count query without /scheme/sys/cpu must report "
           "not_supported");

    const auto models = syscape::cpu::model_names();
    expect(models.error() == syscape::errc::not_supported,
           "model names must report not_supported on Redox OS");

    const auto vendors = syscape::cpu::vendor_identifiers();
    expect(vendors.error() == syscape::errc::not_supported,
           "vendor identifiers must report not_supported on Redox OS");

    const auto phys_cores = syscape::cpu::online_physical_core_count();
    expect(phys_cores.error() == syscape::errc::not_supported,
           "physical core count query must report not_supported on Redox OS");

    const auto packages = syscape::cpu::online_processor_package_count();
    expect(packages.error() == syscape::errc::not_supported,
           "package count query must report not_supported on Redox OS");

    const auto freqs = syscape::cpu::current_frequencies_khz();
    expect(freqs.error() == syscape::errc::not_supported,
           "current frequencies query must report not_supported on Redox OS");

    const auto min_freq = syscape::cpu::minimum_frequency_khz();
    expect(min_freq.error() == syscape::errc::not_supported,
           "minimum frequency query must report not_supported on Redox OS");

    const auto max_freq = syscape::cpu::maximum_frequency_khz();
    expect(max_freq.error() == syscape::errc::not_supported,
           "maximum frequency query must report not_supported on Redox OS");

    const auto isas = syscape::cpu::instruction_set_features();
    expect(isas.error() == syscape::errc::not_supported,
           "instruction set query must report not_supported on Redox OS");

    const auto usage = syscape::cpu::cumulative_processor_usage();
    expect(usage.error() == syscape::errc::not_supported,
           "cumulative processor usage must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_cpu_parsing();
    test_injected_cpu();
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
