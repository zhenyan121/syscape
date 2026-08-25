#include <cassert>
#include <syscape/bluetooth.hpp>

int main() {
    const auto adapters = syscape::bluetooth::adapters();
    assert(!adapters);
    assert(adapters.error() == syscape::errc::not_supported);

    const auto count = syscape::bluetooth::adapter_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto def = syscape::bluetooth::default_adapter();
    assert(!def);
    assert(def.error() == syscape::errc::not_supported);

    const auto paired = syscape::bluetooth::paired_devices();
    assert(!paired);
    assert(paired.error() == syscape::errc::not_supported);

    const auto connected = syscape::bluetooth::connected_devices();
    assert(!connected);
    assert(connected.error() == syscape::errc::not_supported);

    return 0;
}
