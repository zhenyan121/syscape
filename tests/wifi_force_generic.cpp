#include <cassert>
#include <syscape/wifi.hpp>

int main() {
    const auto adapters = syscape::wifi::adapters();
    assert(!adapters);
    assert(adapters.error() == syscape::errc::not_supported);

    const auto count = syscape::wifi::adapter_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto def = syscape::wifi::default_adapter();
    assert(!def);
    assert(def.error() == syscape::errc::not_supported);

    const auto conn = syscape::wifi::current_connection();
    assert(!conn);
    assert(conn.error() == syscape::errc::not_supported);

    const auto configured = syscape::wifi::configured_networks();
    assert(!configured);
    assert(configured.error() == syscape::errc::not_supported);

    return 0;
}
