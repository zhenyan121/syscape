#include <iostream>
#include <syscape/bluetooth.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_windows_bluetooth_backend() {
    const wchar_t unterminated[] = {L'a', L'b', L'c'};
    expect(syscape::detail::bluetooth_backend::bounded_wide_length(
               unterminated, 3U) == 3U,
           "Bounded wide length must not read beyond the array");
    expect(syscape::detail::bluetooth_backend::bounded_wide_length(
               L"ab", 3U) == 2U,
           "Bounded wide length must stop at the terminator");

    const auto adapters = syscape::bluetooth::adapters();
    if (adapters) {
        for (const auto& ad : *adapters) {
            expect(!ad.id.empty(), "Adapter id must not be empty");
            expect(!ad.name.empty(), "Adapter name must not be empty");
        }
    } else {
        expect(static_cast<bool>(adapters.error()),
               "Failure must carry a nonzero error code");
    }

    const auto count = syscape::bluetooth::adapter_count();
    expect(count || static_cast<bool>(count.error()),
           "adapter_count failure must carry an error code");

    const auto def = syscape::bluetooth::default_adapter();
    if (adapters && !adapters->empty()) {
        expect(def.has_value(), "Default adapter must be present when adapters exist");
    }

    const auto paired = syscape::bluetooth::paired_devices();
    expect(paired || static_cast<bool>(paired.error()),
           "paired_devices must return value or error");

    const auto connected = syscape::bluetooth::connected_devices();
    expect(connected || static_cast<bool>(connected.error()),
           "connected_devices must return value or error");
}

} // namespace

int main() {
    test_windows_bluetooth_backend();
    return failures == 0 ? 0 : 1;
}
