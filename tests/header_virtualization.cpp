#include <syscape/virtualization.hpp>

int main() {
    const auto present = syscape::virtualization::is_hypervisor_present();
    static_cast<void>(present);
    return 0;
}
