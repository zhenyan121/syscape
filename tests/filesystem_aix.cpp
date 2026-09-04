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
    expect(mounts.has_value() || mounts.error() == syscape::errc::not_supported,
           "mounts query must succeed or report not_supported");

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
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
