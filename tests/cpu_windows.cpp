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

syscape::detail::cpu_backend::processor_power_information make_record(
    std::uint32_t current_megahertz, std::uint32_t maximum_megahertz) {
    syscape::detail::cpu_backend::processor_power_information record {};
    record.current_megahertz = current_megahertz;
    record.maximum_megahertz = maximum_megahertz;
    return record;
}

struct cache_record {
    relationship_header header;
    ::CACHE_RELATIONSHIP cache;
};

cache_record make_cache_record(unsigned char level,
                               unsigned char associativity,
                               unsigned short line_size,
                               unsigned long bytes,
                               ::PROCESSOR_CACHE_TYPE type,
                               ::KAFFINITY mask) {
    cache_record record {};
    record.header.relationship = RelationCache;
    record.header.size = static_cast<DWORD>(sizeof(cache_record));
    record.cache.Level = level;
    record.cache.Associativity = associativity;
    record.cache.LineSize = line_size;
    record.cache.CacheSize = bytes;
    record.cache.Type = type;
    record.cache.GroupMask.Mask = mask;
    record.cache.GroupMask.Group = 0U;
    return record;
}

bool converts_to(const cache_record& record, std::uint32_t expected_level,
                 syscape::cpu::cache_kind expected_kind,
                 std::uint64_t expected_bytes,
                 std::uint32_t expected_line,
                 std::uint32_t expected_ways,
                 std::uint32_t expected_sets,
                 std::uint32_t expected_shared) {
    const auto converted =
        syscape::detail::cpu_backend::convert_cache_record(
            reinterpret_cast<const unsigned char*>(&record.cache),
            sizeof(record.cache));
    if (!converted) { return false; }
    return converted->level == expected_level &&
           converted->kind == expected_kind &&
           converted->instance_size_bytes == expected_bytes &&
           converted->line_size_bytes == expected_line &&
           converted->associativity_ways == expected_ways &&
           converted->sets_count == expected_sets &&
           converted->shared_logical_processor_count == expected_shared;
}

#if defined(PF_AVX2_INSTRUCTIONS_AVAILABLE) || \
    defined(PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE) || \
    defined(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)
bool has_feature_probe(int identifier, const char* name) {
    for (const auto& probe :
         syscape::detail::cpu_backend::processor_feature_probes) {
        if (probe.identifier == identifier &&
            std::strcmp(probe.name, name) == 0) {
            return true;
        }
    }
    return false;
}
#endif

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

#if defined(PF_AVX2_INSTRUCTIONS_AVAILABLE)
    if (!has_feature_probe(PF_AVX2_INSTRUCTIONS_AVAILABLE, "avx2")) {
        return 43;
    }
#endif
#if defined(PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE)
    if (!has_feature_probe(PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE, "arm-fmac")) {
        return 44;
    }
#endif
#if defined(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)
    if (!has_feature_probe(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE,
                           "armv81-atomic")) {
        return 45;
    }
#endif

    if (backend::count_affinity_bits(0U) != 0U ||
        backend::count_affinity_bits(0xF0U) != 4U ||
        backend::count_affinity_bits(~static_cast<::KAFFINITY>(0U)) !=
            static_cast<std::uint32_t>(sizeof(::KAFFINITY) * 8U)) {
        return 30;
    }

    const cache_record data_cache =
        make_cache_record(1U, 8U, 64U, 32768UL, ::CacheData, 0x3U);
    if (!converts_to(data_cache, 1U,
                     syscape::cpu::cache_kind::data, 32768ULL,
                     64U, 8U, 0U, 2U)) {
        return 31;
    }

    const cache_record instruction_cache = make_cache_record(
        1U, 8U, 64U, 32768UL, ::CacheInstruction, 0x3U);
    if (!converts_to(instruction_cache, 1U,
                     syscape::cpu::cache_kind::instruction,
                     32768ULL, 64U, 8U, 0U, 2U)) {
        return 32;
    }

    const cache_record unified_cache =
        make_cache_record(2U, 16U, 64U, 1048576UL, ::CacheUnified, 0xFU);
    if (!converts_to(unified_cache, 2U,
                     syscape::cpu::cache_kind::unified,
                     1048576ULL, 64U, 16U, 0U, 4U)) {
        return 33;
    }

    // A fully associative cache is exactly one set holding every line.
    const cache_record fully_associative =
        make_cache_record(2U, ::CACHE_FULLY_ASSOCIATIVE, 64U, 65536UL,
                          ::CacheUnified, 0x1U);
    if (!converts_to(fully_associative, 2U,
                     syscape::cpu::cache_kind::unified,
                     65536ULL, 64U, 1024U, 1U, 1U)) {
        return 34;
    }

    const cache_record trace_cache =
        make_cache_record(1U, 8U, 64U, 4096UL, ::CacheTrace, 0x1U);
    if (!converts_to(trace_cache, 1U,
                     syscape::cpu::cache_kind::trace, 4096ULL,
                     64U, 8U, 0U, 1U)) {
        return 35;
    }

    const cache_record zero_level =
        make_cache_record(0U, 8U, 64U, 32768UL, ::CacheData, 0x1U);
    const cache_record zero_line =
        make_cache_record(1U, 8U, 0U, 32768UL, ::CacheData, 0x1U);
    const cache_record zero_size =
        make_cache_record(1U, 8U, 64U, 0UL, ::CacheData, 0x1U);
    const cache_record empty_mask =
        make_cache_record(1U, 8U, 64U, 32768UL, ::CacheData, 0U);
    const cache_record foreign_group =
        make_cache_record(1U, 8U, 64U, 32768UL, ::CacheData, 0x1U);
    foreign_group.cache.GroupMask.Group = 1U;
    const cache_record unknown_type =
        make_cache_record(1U, 8U, 64U, 32768UL,
                          static_cast<::PROCESSOR_CACHE_TYPE>(99), 0x1U);
    const cache_record torn_fully_associative =
        make_cache_record(1U, ::CACHE_FULLY_ASSOCIATIVE, 96U, 32768UL,
                          ::CacheUnified, 0x1U);
    for (const cache_record* malformed :
         {&zero_level, &zero_line, &zero_size, &empty_mask, &foreign_group,
          &unknown_type, &torn_fully_associative}) {
        if (syscape::detail::cpu_backend::convert_cache_record(
                reinterpret_cast<const unsigned char*>(malformed),
                sizeof(cache_record))) {
            return 36;
        }
    }
    if (backend::convert_cache_record(
            reinterpret_cast<const unsigned char*>(&data_cache.cache),
            sizeof(relationship_header))) {
        return 37;
    }

    const syscape::detail::cpu_backend::processor_power_information clocks[] = {
        make_record(800U, 2400U), make_record(1200U, 3100U)};
    const auto currents =
        backend::parse_current_frequencies(clocks, 2U);
    if (!currents || currents->size() != 2U || (*currents)[0] != 800000U ||
        (*currents)[1] != 1200000U) {
        return 10;
    }
    const auto bound = backend::parse_maximum_frequency(clocks, 2U);
    if (!bound || *bound != 3100000U) { return 11; }

    syscape::detail::cpu_backend::processor_power_information zero_clock =
        make_record(0U, 2400U);
    if (backend::parse_current_frequencies(&zero_clock, 1U)) { return 12; }
    syscape::detail::cpu_backend::processor_power_information zero_bound =
        make_record(800U, 0U);
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

    const auto caches = syscape::cpu::cache_descriptors();
    if (!caches) {
        // Multi-group systems report not_supported for the same reason as
        // the other group-relative queries.
        if (caches.error() != std::errc::operation_not_supported) {
            return 38;
        }
    } else {
        if (caches->empty()) { return 39; }
        for (std::size_t position = 1U; position < caches->size();
             ++position) {
            const auto& previous_entry = (*caches)[position - 1U];
            const auto& current_entry = (*caches)[position];
            if (current_entry.level < previous_entry.level ||
                current_entry.instance_size_bytes == 0ULL ||
                current_entry.line_size_bytes == 0U) {
                return 40;
            }
        }
    }

    const auto features = syscape::cpu::instruction_set_features();
    if (!features) { return 41; }
    for (const std::string& identifier : *features) {
        if (identifier.empty()) { return 42; }
    }
    return 0;
}
