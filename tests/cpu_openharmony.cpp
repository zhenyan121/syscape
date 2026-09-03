#include <iostream>
#include <string>
#include <vector>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/openharmony.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_cpu_list_parser() {
    using syscape::detail::cpu_backend::parse_cpu_list;
    const auto single = parse_cpu_list("0");
    expect(single && single->size() == 1U && (*single)[0] == 0U,
           "Single CPU list must parse");

    const auto range = parse_cpu_list("0-3");
    expect(range && range->size() == 4U && (*range)[0] == 0U &&
               (*range)[3] == 3U,
           "Range CPU list must parse");

    const auto non_contiguous = parse_cpu_list("0,2-3,7");
    expect(non_contiguous && non_contiguous->size() == 4U &&
               (*non_contiguous)[0] == 0U && (*non_contiguous)[1] == 2U &&
               (*non_contiguous)[2] == 3U && (*non_contiguous)[3] == 7U,
           "Non-contiguous CPU list must parse accurately");

    const auto empty = parse_cpu_list("");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "Empty CPU list must be malformed");

    const auto garbage = parse_cpu_list("abc");
    expect(!garbage && garbage.error() == syscape::errc::malformed_data,
           "Garbage CPU list must be malformed");
}

void test_frequency_parser() {
    using syscape::detail::cpu_backend::parse_frequency_text;
    const auto valid = parse_frequency_text("1500000\n");
    expect(valid && *valid == 1500000U,
           "Valid frequency must parse to 1500000 kHz");

    const auto zero = parse_frequency_text("0\n");
    expect(!zero && zero.error() == syscape::errc::malformed_data,
           "0 kHz frequency must be rejected as malformed_data");

    const auto junk = parse_frequency_text("1000junk\n");
    expect(!junk && junk.error() == syscape::errc::malformed_data,
           "Trailing text in frequency must be malformed_data");

    const auto empty = parse_frequency_text("   \n");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "Empty frequency must be malformed_data");

    const auto overflow =
        parse_frequency_text("99999999999999999999999999999999\n");
    expect(!overflow && overflow.error() == syscape::errc::value_too_large,
           "Out-of-range frequency must report value_too_large");
}

void test_proc_stat_parser() {
    using syscape::detail::cpu_backend::parse_proc_stat_usage;
    const auto valid =
        parse_proc_stat_usage("cpu  100 20 30 400 5 6 7 0 0 0\n");
    expect(valid && valid->user_ticks == 120U && valid->system_ticks == 43U &&
               valid->idle_ticks == 405U,
           "Valid proc stat line must parse accurately");

    // Construct a simulated /proc/stat exceeding 4096 bytes
    std::string large_stat = "cpu  100 20 30 400 5 6 7 0 0 0\n";
    for (int i = 0; i < 128; ++i) {
        large_stat += "cpu" + std::to_string(i) +
                      " 100 20 30 400 5 6 7 0 0 0 1000 2000 3000 4000\n";
    }
    expect(large_stat.size() > 4096U,
           "Simulated /proc/stat must exceed 4096 bytes");
    const auto large_res = parse_proc_stat_usage(large_stat);
    expect(
        large_res.has_value(),
        "Large /proc/stat buffer (> 4096 bytes) must be parsed successfully");

    const auto malformed_header =
        parse_proc_stat_usage("notcpu 100 20 30 400\n");
    expect(!malformed_header &&
               malformed_header.error() == syscape::errc::malformed_data,
           "Header not starting with 'cpu ' must report malformed_data");
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
    test_cpu_list_parser();
    test_frequency_parser();
    test_proc_stat_parser();
    test_cpu_queries();
    return failures == 0 ? 0 : 1;
}
