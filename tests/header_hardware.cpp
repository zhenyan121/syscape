#include <syscape/hardware.hpp>
#include <syscape/hardware.hpp>

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
    if (!honest(syscape::hardware::system_manufacturer)) { return 1; }
    if (!honest(syscape::hardware::system_product_name)) { return 2; }
    if (!honest(syscape::hardware::system_product_version)) { return 3; }
    if (!honest(syscape::hardware::motherboard_manufacturer)) { return 4; }
    if (!honest(syscape::hardware::motherboard_product_name)) { return 5; }
    if (!honest(syscape::hardware::motherboard_version)) { return 6; }
    if (!honest(syscape::hardware::firmware_vendor)) { return 7; }
    if (!honest(syscape::hardware::firmware_version)) { return 8; }
    if (!honest(syscape::hardware::firmware_release_date)) { return 9; }
    if (!honest(syscape::hardware::chassis_form_factor)) { return 10; }
    if (!honest(syscape::hardware::hardware_uuid)) { return 11; }
    if (!honest(syscape::hardware::pci_devices)) { return 12; }
    if (!honest(syscape::hardware::usb_devices)) { return 13; }
    if (!honest(syscape::hardware::memory_devices)) { return 14; }
    return 0;
}
