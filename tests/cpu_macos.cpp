#include <cstring>
#include <cstdint>
#include <limits>
#include <system_error>

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
    if (!logical || !physical || *logical == 0U || *physical == 0U ||
        *physical > *logical) {
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
    if (static_cast<bool>(minimum) != static_cast<bool>(maximum)) { return 5; }
    if (minimum &&
        (!minimum || !maximum || *minimum == 0U || *maximum < *minimum)) {
        return 6;
    }

    const auto first_usage = syscape::cpu::cumulative_processor_usage();
    const auto second_usage = syscape::cpu::cumulative_processor_usage();
    if (!first_usage || !second_usage ||
        second_usage->user_ticks < first_usage->user_ticks ||
        second_usage->system_ticks < first_usage->system_ticks ||
        second_usage->idle_ticks < first_usage->idle_ticks) {
        return 7;
    }
    return 0;
}
