#include <cassert>
#include <syscape/os.hpp>

int main() {
    const auto ver = syscape::os::product_version();
    assert(ver.has_value());
    assert(*ver == "V7R5M0");

    const auto kver = syscape::os::kernel_version();
    assert(kver.has_value());
    assert(*kver == "V7R5M0");

    return 0;
}
