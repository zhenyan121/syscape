#include <cassert>
#include <syscape/os.hpp>

int main() {
    const auto name = syscape::os::product_name();
    assert(name.has_value());
    assert(*name == "IBM i");

    const auto kname = syscape::os::kernel_name();
    assert(kname.has_value());
    assert(*kname == "PASE for i");

    return 0;
}
