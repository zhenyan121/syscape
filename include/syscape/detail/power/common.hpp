#ifndef SYSCAPE_DETAIL_POWER_COMMON_HPP
#define SYSCAPE_DETAIL_POWER_COMMON_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <syscape/detail/utf8.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace power_common {

using battery_condition = ::syscape::power::battery_state;

/// Tri-state outcome of an external-power-source query, shared by the
/// Hosted backends awaiting boundary conversion.
///
/// Platforms record external power as a three-valued fact: connected,
/// disconnected, or no usable evidence. The public boundary turns
/// no_evidence into the portable not_found error instead of fabricating a
/// Boolean answer.
enum class external_presence {
    /// The platform exposes no acceptable evidence either way.
    no_evidence,
    /// The platform reports at least one active external source.
    connected,
    /// The platform reports that the system draws no external power.
    disconnected
};

/// One recorded battery snapshot shared by the Hosted backends awaiting
/// boundary conversion.
struct battery_record {
    /// Verbatim platform label. An empty identifier is valid data that means
    /// the platform records no name for the battery, for example when it
    /// aggregates several physical packs into one logical view.
    std::string identifier;
    /// Whether the platform reports the battery as physically present.
    bool present = true;
    /// Recorded operating condition.
    battery_condition condition = battery_condition::unknown;
    /// Whether the platform exposed a charge estimate in percent.
    bool has_charge_percent = false;
    /// Platform charge estimate from zero through one hundred.
    std::uint32_t charge_percent = 0U;
    /// Whether the platform tracks completed charge cycles.
    bool has_cycle_count = false;
    /// Completed charge and discharge cycles recorded by the platform.
    std::uint64_t cycle_count = 0U;
};

/// Validates converted battery entries at the public boundary.
///
/// Identifiers must be well-formed UTF-8, because Hosted Full text is UTF-8
/// by contract. A charge estimate outside the documented zero-to-one-
/// hundred range contradicts every platform's definition of the field and
/// is malformed platform data rather than an unusual reading.
inline result<std::vector<battery_record>> validate_battery_records(
    result<std::vector<battery_record>> records) {
    if (!records) { return fail(records.error()); }
    for (const battery_record& record : *records) {
        if (!is_valid_utf8(record.identifier)) {
            return fail(errc::invalid_encoding);
        }
        if (record.has_charge_percent && record.charge_percent > 100U) {
            return fail(errc::malformed_data);
        }
    }
    return records;
}

} // namespace power_common
} // namespace detail
} // namespace syscape

#endif
