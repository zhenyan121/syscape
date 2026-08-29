#ifndef SYSCAPE_DETAIL_CPU_WINDOWS_HPP
#define SYSCAPE_DETAIL_CPU_WINDOWS_HPP

#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0601
#error "syscape/cpu.hpp requires _WIN32_WINNT >= 0x0601 on Windows"
#endif

#if defined(WINVER) && WINVER < 0x0601
#error "syscape/cpu.hpp requires WINVER >= 0x0601 on Windows"
#endif

#if !defined(_WIN32_WINNT)
#define SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#if !defined(WINVER)
#define SYSCAPE_DETAIL_CPU_DEFINED_WINVER
#define WINVER 0x0601
#endif

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <string>
#include <system_error>
#include <vector>
#include <windows.h>
#include <powerbase.h>

#include <syscape/detail/cpu/common.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace cpu_backend {

/// Internal representation of the documented processor power record.
///
/// Microsoft documents this six-ULONG layout but current SDK releases do not
/// consistently declare PROCESSOR_POWER_INFORMATION in a public header.
/// Keeping the representation internal avoids exposing or conditionally
/// redefining a Windows SDK type.
struct processor_power_information {
    ::ULONG number = 0U;
    ::ULONG maximum_megahertz = 0U;
    ::ULONG current_megahertz = 0U;
    ::ULONG limit_megahertz = 0U;
    ::ULONG maximum_idle_state = 0U;
    ::ULONG current_idle_state = 0U;
};

static_assert(sizeof(processor_power_information) == 6U * sizeof(::ULONG),
              "Unexpected Windows processor power record layout");

inline result<std::vector<std::string>> vendor_identifiers() {
    return fail(errc::not_supported);
}

inline result<std::vector<std::string>> model_names() {
    return fail(errc::not_supported);
}

inline result<std::uint32_t> online_logical_processor_count() {
    const DWORD value = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (value == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    return static_cast<std::uint32_t>(value);
}

/// Reads one buffer of the requested relationship records through the
/// documented sizing procedure.
///
/// The documented procedure returns the required size through
/// ERROR_INSUFFICIENT_BUFFER and may need repeated attempts when the
/// platform topology changes between calls; the retries stay capped so the
/// growth loop can never run unbounded.
inline result<std::vector<unsigned char>> query_relationship_records(
    LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
    constexpr DWORD maximum_size = 64U * 1024U * 1024U;
    DWORD size = 0U;
    if (::GetLogicalProcessorInformationEx(relationship, nullptr, &size) !=
        FALSE) {
        return fail(errc::malformed_data);
    }
    const DWORD first_error = ::GetLastError();
    if (first_error != ERROR_INSUFFICIENT_BUFFER) {
        return fail(std::error_code(static_cast<int>(first_error),
                                    std::system_category()));
    }

    for (unsigned int attempt = 0U; attempt < 4U; ++attempt) {
        if (size == 0U || size > maximum_size) {
            return fail(size == 0U ? errc::malformed_data
                                   : errc::resource_exhausted);
        }
        std::vector<unsigned char> buffer(static_cast<std::size_t>(size));
        auto* information = reinterpret_cast<
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
        DWORD returned_size = size;
        if (::GetLogicalProcessorInformationEx(
                relationship, information, &returned_size) != FALSE) {
            if (returned_size == 0U || returned_size > size) {
                return fail(errc::malformed_data);
            }
            buffer.resize(returned_size);
            return buffer;
        }

        const DWORD error = ::GetLastError();
        if (error != ERROR_INSUFFICIENT_BUFFER) {
            return fail(std::error_code(static_cast<int>(error),
                                        std::system_category()));
        }
        if (returned_size <= size) { return fail(errc::malformed_data); }
        size = returned_size;
    }
    return fail(errc::temporarily_unavailable);
}

/// Visits the payload of every record of the requested relationship kind in
/// a returned buffer.
///
/// The buffer holds contiguous variable-length records, each beginning with
/// the documented relationship-and-size header. A record whose size cannot
/// span the header, extends beyond the buffer, or belongs to another
/// relationship kind contradicts the documented layout and is malformed
/// platform data.
template <typename Visitor>
inline result<void> walk_relationship_records(
    const unsigned char* buffer, std::size_t size,
    LOGICAL_PROCESSOR_RELATIONSHIP relationship, Visitor&& visitor) {
    struct relationship_header {
        LOGICAL_PROCESSOR_RELATIONSHIP relationship;
        DWORD size;
    };
    std::size_t offset = 0U;
    while (offset < size) {
        const std::size_t remaining = size - offset;
        if (remaining < sizeof(relationship_header)) {
            return fail(errc::malformed_data);
        }
        relationship_header current {};
        std::memcpy(&current, buffer + offset, sizeof(current));
        if (current.size < sizeof(relationship_header) ||
            current.size > remaining || current.relationship != relationship) {
            return fail(errc::malformed_data);
        }
        const result<void> visited = visitor(
            buffer + offset + sizeof(relationship_header),
            static_cast<std::size_t>(current.size) -
                sizeof(relationship_header));
        if (!visited) { return fail(visited.error()); }
        offset += current.size;
    }
    return result<void>();
}

inline result<std::uint32_t> parse_relationship_count(
    const unsigned char* buffer, DWORD size,
    LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
    std::uint32_t count = 0U;
    const result<void> walked = walk_relationship_records(
        buffer, static_cast<std::size_t>(size), relationship,
        [&count](const unsigned char*, std::size_t) -> result<void> {
            if (count == (std::numeric_limits<std::uint32_t>::max)()) {
                return fail(errc::value_too_large);
            }
            ++count;
            return result<void>();
        });
    if (!walked) { return fail(walked.error()); }
    return count == 0U ? result<std::uint32_t>(fail(errc::malformed_data))
                       : result<std::uint32_t>(count);
}

inline result<std::uint32_t> relationship_count(
    LOGICAL_PROCESSOR_RELATIONSHIP relationship) {
    const result<std::vector<unsigned char>> records =
        query_relationship_records(relationship);
    if (!records) { return fail(records.error()); }
    return parse_relationship_count(records->data(),
                                    static_cast<DWORD>(records->size()),
                                    relationship);
}

inline result<std::uint32_t> online_physical_core_count() {
    return relationship_count(RelationProcessorCore);
}

inline result<std::uint32_t> online_processor_package_count() {
    return relationship_count(RelationProcessorPackage);
}

/// Requires a single active processor group for group-relative APIs.
inline result<void> require_single_active_processor_group();

/// Counts the set bits of one affinity mask without compiler builtins.
inline std::uint32_t count_affinity_bits(::KAFFINITY mask) noexcept {
    std::uint32_t count = 0U;
    while (mask != 0U) {
        count += static_cast<std::uint32_t>(mask & 1U);
        mask >>= 1U;
    }
    return count;
}

/// Converts one documented cache relationship record into the portable
/// representation.
///
/// A fully associative cache is exactly one set holding every line, so its
/// documented CACHE_FULLY_ASSOCIATIVE marker converts to one set with
/// size divided by line size ways; that arithmetic is the documented
/// definition rather than a fabricated value. A zero level, line size, or
/// byte size, an unrecognized cache type, an empty sharing mask, and a
/// nonzero group index under the single-group contract are malformed
/// platform data.
inline result<cpu_common::cache_entry> convert_cache_record(
    const unsigned char* payload, std::size_t payload_size) {
    if (payload_size < sizeof(::CACHE_RELATIONSHIP)) {
        return fail(errc::malformed_data);
    }
    ::CACHE_RELATIONSHIP record {};
    std::memcpy(&record, payload, sizeof(record));

    if (record.Level == 0U || record.LineSize == 0U ||
        record.CacheSize == 0U) {
        return fail(errc::malformed_data);
    }
    cpu_common::cache_kind kind = cpu_common::cache_kind::unified;
    switch (record.Type) {
    case ::CacheData: kind = cpu_common::cache_kind::data; break;
    case ::CacheInstruction:
        kind = cpu_common::cache_kind::instruction;
        break;
    case ::CacheUnified: kind = cpu_common::cache_kind::unified; break;
    case ::CacheTrace: kind = cpu_common::cache_kind::trace; break;
    default: return fail(errc::malformed_data);
    }
    if (record.GroupMask.Group != 0U || record.GroupMask.Mask == 0U) {
        return fail(errc::malformed_data);
    }

    cpu_common::cache_entry entry;
    entry.level = record.Level;
    entry.kind = kind;
    entry.instance_size_bytes = record.CacheSize;
    entry.line_size_bytes = record.LineSize;
    if (record.Associativity == CACHE_FULLY_ASSOCIATIVE) {
        if (record.CacheSize % record.LineSize != 0U) {
            return fail(errc::malformed_data);
        }
        entry.sets_count = 1U;
        entry.associativity_ways = record.CacheSize / record.LineSize;
    } else {
        entry.associativity_ways = record.Associativity;
        entry.sets_count = 0U;
    }
    entry.shared_logical_processor_count =
        count_affinity_bits(record.GroupMask.Mask);
    return entry;
}

/// Enumerates one entry per distinct cache instance, ordered by level and
/// then by kind.
///
/// The documented GetLogicalProcessorInformationEx cache relationship
/// supplies every fact of the portable contract on single-group systems;
/// multi-group systems report not_supported for the same reason as the
/// other group-relative queries in this header.
inline result<std::vector<cpu_common::cache_entry>> cache_descriptors() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    const result<std::vector<unsigned char>> records =
        query_relationship_records(RelationCache);
    if (!records) { return fail(records.error()); }

    std::vector<cpu_common::cache_entry> entries;
    const result<void> walked = walk_relationship_records(
        records->data(), records->size(), RelationCache,
        [&entries](const unsigned char* payload,
                   std::size_t payload_size) -> result<void> {
            const result<cpu_common::cache_entry> converted =
                convert_cache_record(payload, payload_size);
            if (!converted) { return fail(converted.error()); }
            entries.push_back(*converted);
            return result<void>();
        });
    if (!walked) { return fail(walked.error()); }
    if (entries.empty()) { return fail(errc::malformed_data); }
    std::sort(entries.begin(), entries.end(),
              [](const cpu_common::cache_entry& left,
                 const cpu_common::cache_entry& right) noexcept {
                  if (left.level != right.level) {
                      return left.level < right.level;
                  }
                  return left.kind < right.kind;
              });
    return entries;
}

/// One documented processor-feature probe with its rendered identifier.
struct processor_feature_probe {
    /// The documented PF_ constant passed to IsProcessorFeaturePresent.
    int identifier;
    /// The lowercase identifier rendered for a present feature, derived
    /// from the documented constant name.
    const char* name;
};

/// The documented processor-feature enumeration this slice reports.
///
/// Every probe is guarded by its documented constant so that older SDKs
/// simply omit newer facts; the identifiers name instruction-set
/// facilities, while operating-system and firmware facilities exposed by
/// the same interface are outside this contract.
inline constexpr processor_feature_probe processor_feature_probes[] = {
#if defined(PF_COMPARE_EXCHANGE_DOUBLE)
    {PF_COMPARE_EXCHANGE_DOUBLE, "cmpxchg8b"},
#endif
#if defined(PF_COMPARE_EXCHANGE128)
    {PF_COMPARE_EXCHANGE128, "cmpxchg16b"},
#endif
#if defined(PF_COMPARE64_EXCHANGE128)
    {PF_COMPARE64_EXCHANGE128, "cmp8xchg16"},
#endif
#if defined(PF_MMX_INSTRUCTIONS_AVAILABLE)
    {PF_MMX_INSTRUCTIONS_AVAILABLE, "mmx"},
#endif
#if defined(PF_XMMI_INSTRUCTIONS_AVAILABLE)
    {PF_XMMI_INSTRUCTIONS_AVAILABLE, "sse"},
#endif
#if defined(PF_XMMI64_INSTRUCTIONS_AVAILABLE)
    {PF_XMMI64_INSTRUCTIONS_AVAILABLE, "sse2"},
#endif
#if defined(PF_3DNOW_INSTRUCTIONS_AVAILABLE)
    {PF_3DNOW_INSTRUCTIONS_AVAILABLE, "3dnow"},
#endif
#if defined(PF_RDTSC_INSTRUCTION_AVAILABLE)
    {PF_RDTSC_INSTRUCTION_AVAILABLE, "rdtsc"},
#endif
#if defined(PF_SSE3_INSTRUCTIONS_AVAILABLE)
    {PF_SSE3_INSTRUCTIONS_AVAILABLE, "sse3"},
#endif
#if defined(PF_SSSE3_INSTRUCTIONS_AVAILABLE)
    {PF_SSSE3_INSTRUCTIONS_AVAILABLE, "ssse3"},
#endif
#if defined(PF_SSE4_1_INSTRUCTIONS_AVAILABLE)
    {PF_SSE4_1_INSTRUCTIONS_AVAILABLE, "sse4.1"},
#endif
#if defined(PF_SSE4_2_INSTRUCTIONS_AVAILABLE)
    {PF_SSE4_2_INSTRUCTIONS_AVAILABLE, "sse4.2"},
#endif
#if defined(PF_AVX_INSTRUCTIONS_AVAILABLE)
    {PF_AVX_INSTRUCTIONS_AVAILABLE, "avx"},
#endif
#if defined(PF_AVX2_INSTRUCTIONS_AVAILABLE)
    {PF_AVX2_INSTRUCTIONS_AVAILABLE, "avx2"},
#endif
#if defined(PF_AVX512F_INSTRUCTIONS_AVAILABLE)
    {PF_AVX512F_INSTRUCTIONS_AVAILABLE, "avx512f"},
#endif
#if defined(PF_XSAVE_ENABLED)
    {PF_XSAVE_ENABLED, "xsave"},
#endif
#if defined(PF_RDRAND_INSTRUCTION_AVAILABLE)
    {PF_RDRAND_INSTRUCTION_AVAILABLE, "rdrand"},
#endif
#if defined(PF_RDWRFSGSBASE_AVAILABLE)
    {PF_RDWRFSGSBASE_AVAILABLE, "rdwrfsgsbase"},
#endif
#if defined(PF_RDTSCP_INSTRUCTION_AVAILABLE)
    {PF_RDTSCP_INSTRUCTION_AVAILABLE, "rdtscp"},
#endif
#if defined(PF_RDPID_AVAILABLE)
    {PF_RDPID_AVAILABLE, "rdpid"},
#endif
#if defined(PF_ERMS_AVAILABLE)
    {PF_ERMS_AVAILABLE, "erms"},
#endif
#if defined(PF_BMI2_INSTRUCTIONS_AVAILABLE)
    {PF_BMI2_INSTRUCTIONS_AVAILABLE, "bmi2"},
#endif
#if defined(PF_MOVDIR64B_INSTRUCTION_AVAILABLE)
    {PF_MOVDIR64B_INSTRUCTION_AVAILABLE, "movdir64b"},
#endif
#if defined(PF_UMONITOR_INSTRUCTION_AVAILABLE)
    {PF_UMONITOR_INSTRUCTION_AVAILABLE, "umonitor"},
#endif
#if defined(PF_ARM_NEON_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_NEON_INSTRUCTIONS_AVAILABLE, "arm-neon"},
#endif
#if defined(PF_ARM_VFP_32_REGISTERS_AVAILABLE)
    {PF_ARM_VFP_32_REGISTERS_AVAILABLE, "arm-vfp32"},
#endif
#if defined(PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE)
    {PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE, "arm-divide"},
#endif
#if defined(PF_ARM_64BIT_LOADSTORE_ATOMIC)
    {PF_ARM_64BIT_LOADSTORE_ATOMIC, "arm-64bit-loadstore-atomic"},
#endif
#if defined(PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE, "arm-fmac"},
#endif
#if defined(PF_ARM_V8_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V8_INSTRUCTIONS_AVAILABLE, "armv8"},
#endif
#if defined(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE, "armv8-crypto"},
#endif
#if defined(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE, "armv8-crc32"},
#endif
#if defined(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE, "armv81-atomic"},
#endif
#if defined(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE, "armv82-dp"},
#endif
#if defined(PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE, "armv83-jscvt"},
#endif
#if defined(PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE, "armv83-lrcpc"},
#endif
#if defined(PF_ARM_SVE_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_INSTRUCTIONS_AVAILABLE, "arm-sve"},
#endif
#if defined(PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE, "arm-sve2"},
#endif
#if defined(PF_ARM_SVE2_1_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE2_1_INSTRUCTIONS_AVAILABLE, "arm-sve2.1"},
#endif
#if defined(PF_ARM_SVE_AES_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_AES_INSTRUCTIONS_AVAILABLE, "arm-sve-aes"},
#endif
#if defined(PF_ARM_SVE_PMULL128_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_PMULL128_INSTRUCTIONS_AVAILABLE, "arm-sve-pmull128"},
#endif
#if defined(PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE, "arm-sve-bitperm"},
#endif
#if defined(PF_ARM_SVE_BF16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_BF16_INSTRUCTIONS_AVAILABLE, "arm-sve-bf16"},
#endif
#if defined(PF_ARM_SVE_EBF16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_EBF16_INSTRUCTIONS_AVAILABLE, "arm-sve-ebf16"},
#endif
#if defined(PF_ARM_SVE_B16B16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_B16B16_INSTRUCTIONS_AVAILABLE, "arm-sve-b16b16"},
#endif
#if defined(PF_ARM_SVE_SHA3_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_SHA3_INSTRUCTIONS_AVAILABLE, "arm-sve-sha3"},
#endif
#if defined(PF_ARM_SVE_SM4_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_SM4_INSTRUCTIONS_AVAILABLE, "arm-sve-sm4"},
#endif
#if defined(PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE, "arm-sve-i8mm"},
#endif
#if defined(PF_ARM_SVE_F32MM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_F32MM_INSTRUCTIONS_AVAILABLE, "arm-sve-f32mm"},
#endif
#if defined(PF_ARM_SVE_F64MM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SVE_F64MM_INSTRUCTIONS_AVAILABLE, "arm-sve-f64mm"},
#endif
#if defined(PF_ARM_LSE2_AVAILABLE)
    {PF_ARM_LSE2_AVAILABLE, "arm-lse2"},
#endif
#if defined(PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE, "arm-sha3"},
#endif
#if defined(PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE, "arm-sha512"},
#endif
#if defined(PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE, "armv82-i8mm"},
#endif
#if defined(PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE, "armv82-fp16"},
#endif
#if defined(PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE, "armv86-bf16"},
#endif
#if defined(PF_ARM_V86_EBF16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_V86_EBF16_INSTRUCTIONS_AVAILABLE, "armv86-ebf16"},
#endif
#if defined(PF_ARM_SME_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_INSTRUCTIONS_AVAILABLE, "arm-sme"},
#endif
#if defined(PF_ARM_SME2_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME2_INSTRUCTIONS_AVAILABLE, "arm-sme2"},
#endif
#if defined(PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE, "arm-sme2.1"},
#endif
#if defined(PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE, "arm-sme2.2"},
#endif
#if defined(PF_ARM_SME_AES_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_AES_INSTRUCTIONS_AVAILABLE, "arm-sme-aes"},
#endif
#if defined(PF_ARM_SME_SBITPERM_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SBITPERM_INSTRUCTIONS_AVAILABLE, "arm-sme-sbitperm"},
#endif
#if defined(PF_ARM_SME_SF8MM4_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SF8MM4_INSTRUCTIONS_AVAILABLE, "arm-sme-sf8mm4"},
#endif
#if defined(PF_ARM_SME_SF8MM8_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SF8MM8_INSTRUCTIONS_AVAILABLE, "arm-sme-sf8mm8"},
#endif
#if defined(PF_ARM_SME_SF8DP2_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SF8DP2_INSTRUCTIONS_AVAILABLE, "arm-sme-sf8dp2"},
#endif
#if defined(PF_ARM_SME_SF8DP4_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SF8DP4_INSTRUCTIONS_AVAILABLE, "arm-sme-sf8dp4"},
#endif
#if defined(PF_ARM_SME_SF8FMA_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_SF8FMA_INSTRUCTIONS_AVAILABLE, "arm-sme-sf8fma"},
#endif
#if defined(PF_ARM_SME_F8F32_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_F8F32_INSTRUCTIONS_AVAILABLE, "arm-sme-f8f32"},
#endif
#if defined(PF_ARM_SME_F8F16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_F8F16_INSTRUCTIONS_AVAILABLE, "arm-sme-f8f16"},
#endif
#if defined(PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE, "arm-sme-f16f16"},
#endif
#if defined(PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE, "arm-sme-b16b16"},
#endif
#if defined(PF_ARM_SME_F64F64_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_F64F64_INSTRUCTIONS_AVAILABLE, "arm-sme-f64f64"},
#endif
#if defined(PF_ARM_SME_I16I64_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_I16I64_INSTRUCTIONS_AVAILABLE, "arm-sme-i16i64"},
#endif
#if defined(PF_ARM_SME_LUTv2_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_LUTv2_INSTRUCTIONS_AVAILABLE, "arm-sme-lutv2"},
#endif
#if defined(PF_ARM_SME_FA64_INSTRUCTIONS_AVAILABLE)
    {PF_ARM_SME_FA64_INSTRUCTIONS_AVAILABLE, "arm-sme-fa64"},
#endif
};

inline result<std::vector<std::string>> instruction_set_features() {
    std::vector<std::string> features;
    for (const processor_feature_probe& probe : processor_feature_probes) {
        if (::IsProcessorFeaturePresent(probe.identifier) != FALSE) {
            features.emplace_back(probe.name);
        }
    }
    return features;
}

/// Requires a single active processor group for legacy group-relative APIs.
///
/// A zero group count is the documented failure sentinel. More than one group
/// means that neither GetSystemTimes nor the documented sizing procedure for
/// CallNtPowerInformation can satisfy a system-wide contract.
inline bool group_count_covers_system(std::size_t active_group_count) noexcept {
    return active_group_count == 1U;
}

inline result<void> require_single_active_processor_group() {
    const WORD active_group_count = ::GetActiveProcessorGroupCount();
    if (active_group_count == 0U) { return fail(errc::io_error); }
    return group_count_covers_system(active_group_count)
               ? result<void>()
               : result<void>(fail(errc::not_supported));
}

/// Converts the documented PROCESSOR_POWER_INFORMATION records into current
/// per-processor frequencies in kilohertz.
///
/// The API reports whole megahertz values, so each value multiplies by one
/// thousand exactly. A zero frequency cannot describe an operating
/// processor and is malformed platform data.
inline result<std::vector<std::uint32_t>> parse_current_frequencies(
    const processor_power_information* information, std::size_t count) {
    if (!information || count == 0U) { return fail(errc::malformed_data); }
    std::vector<std::uint32_t> values;
    values.reserve(count);
    for (std::size_t index = 0U; index < count; ++index) {
        const std::uint64_t megahertz =
            static_cast<std::uint64_t>(
                information[index].current_megahertz);
        if (megahertz == 0U) { return fail(errc::malformed_data); }
        const std::uint64_t kilohertz = megahertz * 1000U;
        if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        values.push_back(static_cast<std::uint32_t>(kilohertz));
    }
    return values;
}

/// Extracts the highest recorded processor clock in kilohertz from the
/// documented PROCESSOR_POWER_INFORMATION records.
inline result<std::uint32_t> parse_maximum_frequency(
    const processor_power_information* information, std::size_t count) {
    if (!information || count == 0U) { return fail(errc::malformed_data); }
    std::uint64_t maximum_megahertz = 0U;
    for (std::size_t index = 0U; index < count; ++index) {
        maximum_megahertz =
            maximum_megahertz < information[index].maximum_megahertz
                ? static_cast<std::uint64_t>(
                      information[index].maximum_megahertz)
                : maximum_megahertz;
    }
    if (maximum_megahertz == 0U) { return fail(errc::malformed_data); }
    const std::uint64_t kilohertz = maximum_megahertz * 1000U;
    if (kilohertz > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return static_cast<std::uint32_t>(kilohertz);
}

/// Reads one buffer of processor power records through the documented power
/// information interface.
///
/// The NTSTATUS result carries no standard-library error category, so a
/// failing status maps to io_error for the same reason Mach failures do on
/// other backends.
inline std::error_code processor_power_error(::NTSTATUS status) noexcept {
    // STATUS_ACCESS_DENIED is the documented NTSTATUS value 0xC0000022.
    // Converting the signed status to ULONG preserves its 32-bit value using
    // the standard unsigned conversion rules without requiring ntstatus.h.
    constexpr ::ULONG access_denied_status = 0xC0000022UL;
    return static_cast<::ULONG>(status) == access_denied_status
               ? make_error_code(errc::permission_denied)
               : make_error_code(errc::io_error);
}

inline result<std::size_t> query_processor_power_information(
    unsigned char* buffer, std::size_t byte_count) {
    const ::NTSTATUS status = ::CallNtPowerInformation(
        ProcessorInformation, nullptr, 0U, buffer,
        static_cast<::ULONG>(byte_count));
    if (status != 0) { return fail(processor_power_error(status)); }
    return byte_count / sizeof(processor_power_information);
}

inline result<std::vector<std::uint32_t>> current_frequencies_khz() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    const DWORD processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (processors == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    const std::size_t byte_count =
        static_cast<std::size_t>(processors) *
        sizeof(processor_power_information);
    std::unique_ptr<unsigned char[]> buffer(
        new (std::nothrow) unsigned char[byte_count]);
    if (!buffer) { return fail(errc::resource_exhausted); }

    const result<std::size_t> returned =
        query_processor_power_information(buffer.get(), byte_count);
    if (!returned) { return fail(returned.error()); }
    if (*returned != processors) { return fail(errc::malformed_data); }

    return parse_current_frequencies(
        reinterpret_cast<const processor_power_information*>(buffer.get()),
        *returned);
}

inline result<std::uint32_t> minimum_frequency_khz() {
    // The processor power information records expose no minimum operating
    // frequency, and Windows documents no other public source for that
    // contract.
    return fail(errc::not_supported);
}

inline result<std::uint32_t> maximum_frequency_khz() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    const DWORD processors = ::GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
    if (processors == 0U) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    const std::size_t byte_count =
        static_cast<std::size_t>(processors) *
        sizeof(processor_power_information);
    std::unique_ptr<unsigned char[]> buffer(
        new (std::nothrow) unsigned char[byte_count]);
    if (!buffer) { return fail(errc::resource_exhausted); }

    const result<std::size_t> returned =
        query_processor_power_information(buffer.get(), byte_count);
    if (!returned) { return fail(returned.error()); }
    if (*returned != processors) { return fail(errc::malformed_data); }

    return parse_maximum_frequency(
        reinterpret_cast<const processor_power_information*>(buffer.get()),
        *returned);
}

/// Folds the GetSystemTimes idle, kernel, and user totals into the portable
/// usage buckets.
///
/// The kernel total includes idle time by documentation, so system time is
/// the kernel total minus idle; a kernel total below idle contradicts the
/// documented invariant and is malformed platform data. All three inputs
/// are cumulative hundred-nanosecond counts.
inline result<cpu_common::usage_information> convert_system_times(
    std::uint64_t idle_ticks, std::uint64_t kernel_ticks,
    std::uint64_t user_ticks) {
    if (kernel_ticks < idle_ticks) { return fail(errc::malformed_data); }
    cpu_common::usage_information usage;
    usage.user_ticks = user_ticks;
    usage.system_ticks = kernel_ticks - idle_ticks;
    usage.idle_ticks = idle_ticks;
    return usage;
}

inline std::uint64_t filetime_to_uint64(const ::FILETIME& value) noexcept {
    ::ULARGE_INTEGER converted {};
    converted.LowPart = value.dwLowDateTime;
    converted.HighPart = value.dwHighDateTime;
    return converted.QuadPart;
}

inline result<cpu_common::usage_information> cumulative_processor_usage() {
    const result<void> group = require_single_active_processor_group();
    if (!group) { return fail(group.error()); }
    ::FILETIME idle_time {};
    ::FILETIME kernel_time {};
    ::FILETIME user_time {};
    if (::GetSystemTimes(&idle_time, &kernel_time, &user_time) == FALSE) {
        return fail(std::error_code(static_cast<int>(::GetLastError()),
                                    std::system_category()));
    }
    return convert_system_times(filetime_to_uint64(idle_time),
                                filetime_to_uint64(kernel_time),
                                filetime_to_uint64(user_time));
}

} // namespace cpu_backend
} // namespace detail
} // namespace syscape

#if defined(SYSCAPE_DETAIL_CPU_DEFINED_WINVER)
#undef WINVER
#undef SYSCAPE_DETAIL_CPU_DEFINED_WINVER
#endif

#if defined(SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT)
#undef _WIN32_WINNT
#undef SYSCAPE_DETAIL_CPU_DEFINED_WIN32_WINNT
#endif

#endif
