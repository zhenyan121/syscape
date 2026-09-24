#include <cassert>
#include <syscape/filesystem.hpp>
#include <syscape/os.hpp>

int main() {
    const auto name = syscape::os::product_name();
    assert(name.has_value());
    assert(*name == "MorphOS");

    const auto ver = syscape::os::product_version();
    assert(ver.has_value());
    assert(*ver == "3.18");

    const auto kname = syscape::os::kernel_name();
    assert(kname.has_value());
    assert(*kname == "Quark");

    const auto max_comp = syscape::filesystem::max_component_length("SYS:");
    assert(max_comp.has_value());
    assert(max_comp->length == 107U);

    return 0;
}
