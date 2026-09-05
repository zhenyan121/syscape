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
    expect(mounts.error() == syscape::errc::not_supported,
           "mounts must report not_supported on Redox OS");

    const auto root_space = syscape::filesystem::space("/");
    expect(root_space && root_space->capacity_bytes > 0,
           "root filesystem space query must succeed with positive capacity "
           "bytes");

    const auto root_id = syscape::filesystem::volume_id("/");
    expect(root_id.has_value(), "root filesystem volume id query must succeed");

    const auto max_comp = syscape::filesystem::max_component_length("/");
    expect(max_comp.has_value(), "max component length query must succeed");

    const auto max_path = syscape::filesystem::max_path_length("/");
    expect(max_path.has_value(), "max path length query must succeed");

    const auto bad_comp =
        syscape::filesystem::max_component_length("/nonexistent_path_xyz_123");
    expect(!bad_comp && bad_comp.error() == syscape::errc::not_found,
           "max component length on nonexistent path must report not_found");

    const auto bad_path =
        syscape::filesystem::max_path_length("/nonexistent_path_xyz_123");
    expect(!bad_path && bad_path.error() == syscape::errc::not_found,
           "max path length on nonexistent path must report not_found");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
