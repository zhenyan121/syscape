#include <cassert>
#include <syscape/sensor.hpp>

int main() {
    const auto temps = syscape::sensor::temperatures();
    assert(!temps);
    assert(temps.error() == syscape::errc::not_supported);

    const auto fans = syscape::sensor::fans();
    assert(!fans);
    assert(fans.error() == syscape::errc::not_supported);

    const auto zones = syscape::sensor::thermal_zones();
    assert(!zones);
    assert(zones.error() == syscape::errc::not_supported);

    return 0;
}
