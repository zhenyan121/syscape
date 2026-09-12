#include <iostream>

#include <syscape/input.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_input_queries() {
    const auto d = syscape::input::devices();
    expect(!d && d.error() == syscape::errc::not_supported,
           "devices must report not_supported on INTEGRITY");

    const auto k = syscape::input::keyboards();
    expect(!k && k.error() == syscape::errc::not_supported,
           "keyboards must report not_supported on INTEGRITY");

    const auto m = syscape::input::mice();
    expect(!m && m.error() == syscape::errc::not_supported,
           "mice must report not_supported on INTEGRITY");

    const auto t = syscape::input::touch_devices();
    expect(!t && t.error() == syscape::errc::not_supported,
           "touch_devices must report not_supported on INTEGRITY");

    const auto g = syscape::input::gamepads();
    expect(!g && g.error() == syscape::errc::not_supported,
           "gamepads must report not_supported on INTEGRITY");

    const auto count = syscape::input::device_count();
    expect(!count && count.error() == syscape::errc::not_supported,
           "device_count must report not_supported on INTEGRITY");
}

} // namespace

int main() {
    test_input_queries();
    return failures == 0 ? 0 : 1;
}
