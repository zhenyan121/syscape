#include <cassert>
#include <syscape/virtualization.hpp>

int main() {
    const auto wine_res = syscape::virtualization::is_wine();
    if (wine_res) {
        if (*wine_res) {
            const auto ver = syscape::virtualization::wine_version();
            assert(ver.has_value());
            assert(!ver->empty());
            const auto build = syscape::virtualization::wine_build_id();
            assert(build.has_value());
            assert(!build->empty());
        } else {
            const auto ver = syscape::virtualization::wine_version();
            assert(!ver.has_value());
            assert(ver.error() == syscape::errc::not_found);
            const auto build = syscape::virtualization::wine_build_id();
            assert(!build.has_value());
            assert(build.error() == syscape::errc::not_found);
        }
    } else {
        assert(wine_res.error() == syscape::errc::not_supported);
    }
    return 0;
}
