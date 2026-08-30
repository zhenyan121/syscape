#include <cstring>
#include <cstdint>
#include <limits>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/common.hpp>
#include <syscape/detail/cpu/macos.hpp>

namespace {

bool usage_equals(const syscape::detail::cpu_common::usage_information& value,
                  std::uint64_t user, std::uint64_t system,
                  std::uint64_t idle) {
    return value.user_ticks == user && value.system_ticks == system &&
           value.idle_ticks == idle;
}

void store_tick(integer_t& destination, natural_t value) {
    static_assert(sizeof(destination) == sizeof(value),
                  "Mach processor tick types must have equal size");
    std::memcpy(&destination, &value, sizeof(value));
}

} // namespace

int main() {
    namespace backend = syscape::detail::cpu_backend;

    const auto environmental_failure = [](const std::error_code& error) {
        return error.category() == std::generic_category() ||
               error == syscape::errc::not_supported ||
               error == syscape::errc::permission_denied ||
               error == syscape::errc::not_found ||
               error == syscape::errc::temporarily_unavailable ||
               error == syscape::errc::malformed_data ||
               error == syscape::errc::io_error ||
               error == syscape::errc::value_too_large ||
               error == syscape::errc::resource_exhausted;
    };

    integer_t records[2 * CPU_STATE_MAX] = {};
    records[CPU_STATE_USER] = 100;
    records[CPU_STATE_NICE] = 10;
    records[CPU_STATE_SYSTEM] = 30;
    records[CPU_STATE_IDLE] = 400;
    records[CPU_STATE_MAX + CPU_STATE_USER] = 50;
    records[CPU_STATE_MAX + CPU_STATE_NICE] = 5;
    records[CPU_STATE_MAX + CPU_STATE_SYSTEM] = 20;
    records[CPU_STATE_MAX + CPU_STATE_IDLE] = 200;

    const auto summed = backend::sum_processor_load(records, 2U);
    if (!summed || !usage_equals(*summed, 165U, 50U, 600U)) { return 1; }

    if (backend::sum_processor_load(nullptr, 1U) ||
        backend::sum_processor_load(records, 0U)) {
        return 2;
    }

    integer_t high_bit[CPU_STATE_MAX] = {};
    const natural_t high_tick =
        static_cast<natural_t>((std::numeric_limits<integer_t>::max)()) + 1U;
    store_tick(high_bit[CPU_STATE_USER], high_tick);
    const auto high_value = backend::sum_processor_load(high_bit, 1U);
    if (!high_value || !usage_equals(*high_value, high_tick, 0U, 0U)) {
        return 8;
    }

    const auto logical = syscape::cpu::online_logical_processor_count();
    const auto physical = syscape::cpu::online_physical_core_count();
    if ((!logical && !environmental_failure(logical.error())) ||
        (!physical && !environmental_failure(physical.error())) ||
        (logical && *logical == 0U) || (physical && *physical == 0U) ||
        (logical && physical && *physical > *logical)) {
        return 3;
    }

    if (syscape::cpu::online_processor_package_count().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::vendor_identifiers().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::model_names().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::current_frequencies_khz().error() !=
            std::errc::operation_not_supported) {
        return 4;
    }

    // Clock bounds follow the platform: Intel Macs expose the sysctl values,
    // Apple silicon does not. Both outcomes are acceptable here.
    const auto minimum = syscape::cpu::minimum_frequency_khz();
    const auto maximum = syscape::cpu::maximum_frequency_khz();
    if ((!minimum && !environmental_failure(minimum.error())) ||
        (!maximum && !environmental_failure(maximum.error()))) {
        return 5;
    }
    if ((minimum && *minimum == 0U) || (maximum && *maximum == 0U) ||
        (minimum && maximum && *maximum < *minimum)) {
        return 6;
    }

    const auto first_usage = syscape::cpu::cumulative_processor_usage();
    const auto second_usage = syscape::cpu::cumulative_processor_usage();
    if ((!first_usage && !environmental_failure(first_usage.error())) ||
        (!second_usage && !environmental_failure(second_usage.error()))) {
        return 7;
    }

    std::vector<std::string> tokens;
    backend::append_whitespace_tokens(tokens, std::string(" fpu sse2"));
    backend::append_whitespace_tokens(tokens, std::string("\tsse2\t avx2\n"));
    backend::append_whitespace_tokens(tokens, std::string("  "));
    const std::vector<std::string> expected{"fpu", "sse2", "avx2"};
    if (tokens != expected) { return 10; }
    if (backend::optional_flag_is_present(0) ||
        !backend::optional_flag_is_present(1) ||
        !backend::optional_flag_is_present(2)) {
        return 18;
    }

    const auto caches = syscape::cpu::cache_descriptors();
    if (caches || caches.error() != std::errc::operation_not_supported) {
        return 11;
    }

    const auto features = syscape::cpu::instruction_set_features();
    if (!features && !environmental_failure(features.error())) {
        return 15;
    }
    if (features) {
        for (const std::string& identifier : *features) {
            if (identifier.empty()) {
                return 16;
            }
            std::size_t seen = 0U;
            for (const std::string& other : *features) {
                if (other == identifier) {
                    ++seen;
                }
            }
            if (seen != 1U) {
                return 17;
            }
        }
    }
    return 0;
}
