#include <iostream>
#include <string>

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
    const auto mounts = syscape::filesystem::mounts();
    expect(!mounts && mounts.error() == syscape::errc::not_supported,
           "mounts must report not_supported on ThreadX");

    const auto root_space = syscape::filesystem::space("/");
    expect(!root_space && root_space.error() == syscape::errc::not_supported,
           "filesystem space must report not_supported on ThreadX");

    const auto root_id = syscape::filesystem::volume_id("/");
    expect(!root_id && root_id.error() == syscape::errc::not_supported,
           "volume id query must report not_supported on ThreadX");

    const auto max_comp = syscape::filesystem::max_component_length("/");
    expect(!max_comp && max_comp.error() == syscape::errc::not_supported,
           "max component length query must report not_supported on ThreadX");

    const auto max_path = syscape::filesystem::max_path_length("/");
    expect(!max_path && max_path.error() == syscape::errc::not_supported,
           "max path length query must report not_supported on ThreadX");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
