#include <cstdint>
#include <cstring>
#include <system_error>
#include <vector>

#include <windows.h>

#include <syscape/cpu.hpp>
#include <syscape/detail/cpu/windows.hpp>

namespace {

struct relationship_header {
    LOGICAL_PROCESSOR_RELATIONSHIP relationship;
    DWORD size;
};

::PROCESSOR_POWER_INFORMATION make_record(std::uint32_t current_megahertz,
                                          std::uint32_t maximum_megahertz) {
    ::PROCESSOR_POWER_INFORMATION record {};
    record.CurrentMhz = current_megahertz;
    record.MaxMhz = maximum_megahertz;
    return record;
}

} // namespace

int main() {
    relationship_header records[2] = {
        {RelationProcessorCore, static_cast<DWORD>(sizeof(relationship_header))},
        {RelationProcessorCore, static_cast<DWORD>(sizeof(relationship_header))}};
    const auto parsed = syscape::detail::cpu_backend::parse_relationship_count(
        reinterpret_cast<const unsigned char*>(records),
        static_cast<DWORD>(sizeof(records)),
        RelationProcessorCore);
    if (!parsed || *parsed != 2U) { return 1; }

    relationship_header malformed = {RelationProcessorCore, 0U};
    if (syscape::detail::cpu_backend::parse_relationship_count(
            reinterpret_cast<const unsigned char*>(&malformed),
            static_cast<DWORD>(sizeof(malformed)), RelationProcessorCore)) {
        return 2;
    }

    namespace backend = syscape::detail::cpu_backend;

    const ::PROCESSOR_POWER_INFORMATION clocks[] = {
        make_record(800U, 2400U), make_record(1200U, 3100U)};
    const auto currents =
        backend::parse_current_frequencies(clocks, 2U);
    if (!currents || currents->size() != 2U || (*currents)[0] != 800000U ||
        (*currents)[1] != 1200000U) {
        return 10;
    }
    const auto bound = backend::parse_maximum_frequency(clocks, 2U);
    if (!bound || *bound != 3100000U) { return 11; }

    ::PROCESSOR_POWER_INFORMATION zero_clock = make_record(0U, 2400U);
    if (backend::parse_current_frequencies(&zero_clock, 1U)) { return 12; }
    ::PROCESSOR_POWER_INFORMATION zero_bound = make_record(800U, 0U);
    if (backend::parse_maximum_frequency(&zero_bound, 1U)) { return 13; }
    if (backend::parse_current_frequencies(nullptr, 1U) ||
        backend::parse_maximum_frequency(clocks, 0U)) {
        return 14;
    }

    const auto usage =
        backend::convert_system_times(100U, 350U, 150U);
    if (!usage || usage->user_ticks != 150U ||
        usage->system_ticks != 250U || usage->idle_ticks != 100U) {
        return 15;
    }
    if (backend::convert_system_times(400U, 350U, 150U)) { return 16; }

    if (backend::group_count_covers_system(0U) ||
        !backend::group_count_covers_system(1U) ||
        backend::group_count_covers_system(2U)) {
        return 17;
    }

    const auto denied = backend::processor_power_error(
        static_cast<::NTSTATUS>(-1073741790L));
    if (denied != std::errc::permission_denied ||
        backend::processor_power_error(static_cast<::NTSTATUS>(-1L)) !=
            std::errc::io_error) {
        return 17;
    }

    const auto minimum = syscape::cpu::minimum_frequency_khz();
    if (minimum.error() != std::errc::operation_not_supported) { return 17; }

    const auto live_currents = syscape::cpu::current_frequencies_khz();
    const auto logical = syscape::cpu::online_logical_processor_count();
    if (!live_currents || !logical || live_currents->size() != *logical) {
        return 18;
    }
    for (const std::uint32_t value : *live_currents) {
        if (value == 0U) { return 19; }
    }

    const auto first_usage = syscape::cpu::cumulative_processor_usage();
    if (first_usage) {
        const auto second_usage = syscape::cpu::cumulative_processor_usage();
        if (!second_usage ||
            second_usage->user_ticks < first_usage->user_ticks ||
            second_usage->system_ticks < first_usage->system_ticks ||
            second_usage->idle_ticks < first_usage->idle_ticks) {
            return 20;
        }
    } else if (first_usage.error() !=
               std::errc::operation_not_supported) {
        // Multi-group systems report not_supported because GetSystemTimes
        // covers only one processor group there.
        return 21;
    }

    const auto physical = syscape::cpu::online_physical_core_count();
    const auto packages = syscape::cpu::online_processor_package_count();
    if (!physical || !packages || *packages > *physical ||
        *physical > *logical) {
        return 3;
    }
    if (syscape::cpu::vendor_identifiers().error() !=
            std::errc::operation_not_supported ||
        syscape::cpu::model_names().error() !=
            std::errc::operation_not_supported) {
        return 4;
    }
    return 0;
}
