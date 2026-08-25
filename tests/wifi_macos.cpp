#include <cassert>
#include <syscape/wifi.hpp>

int main() {
    const auto adapters = syscape::wifi::adapters();
    assert(!adapters);
    assert(adapters.error() == syscape::errc::not_supported);
    return 0;
}
