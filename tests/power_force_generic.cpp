#include <cstdint>
#include <system_error>

#include <syscape/power.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::power::batteries()) &&
                   unsupported(syscape::power::power_sources()) &&
                   unsupported(syscape::power::external_power_online()) &&
                   unsupported(syscape::power::seconds_until_empty())
               ? 0
               : 1;
}
