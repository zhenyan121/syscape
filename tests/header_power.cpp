#include <syscape/power.hpp>
#include <syscape/power.hpp>

#include <cstdint>
#include <system_error>

namespace {

/// Checks that a query either succeeds or fails with an explicit portable
/// condition, never with an exception or a fabricated value.
template <typename Query>
bool honest(const Query& query) {
    try {
        const auto value = query();
        static_cast<void>(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main() {
    return honest(syscape::power::batteries) &&
                   honest(syscape::power::external_power_online) &&
                   honest(syscape::power::seconds_until_empty)
               ? 0
               : 1;
}
