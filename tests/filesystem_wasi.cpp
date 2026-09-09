#include <iostream>

#include <syscape/filesystem.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_filesystem_queries() {
    const auto m = syscape::filesystem::mounts();
    expect(!m && m.error() == syscape::errc::not_supported,
           "mounts must report not_supported on WASI");

    const auto sp = syscape::filesystem::space("/");
    expect(!sp && sp.error() == syscape::errc::not_supported,
           "space must report not_supported on WASI");

    const auto vol = syscape::filesystem::volume_id("/");
    expect(!vol && vol.error() == syscape::errc::not_supported,
           "volume_id must report not_supported on WASI");

    const auto max_comp = syscape::filesystem::max_component_length("/");
    expect(!max_comp && max_comp.error() == syscape::errc::not_supported,
           "max_component_length must report not_supported on WASI");

    const auto max_path = syscape::filesystem::max_path_length("/");
    expect(!max_path && max_path.error() == syscape::errc::not_supported,
           "max_path_length must report not_supported on WASI");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
