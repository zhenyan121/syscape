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
    const auto mounts = syscape::filesystem::mounts();
    expect(mounts && !mounts->empty(),
           "mounts query must return at least one mount");

    const auto space = syscape::filesystem::space("/");
    expect(space && space->capacity_bytes > 0U,
           "root filesystem total bytes must be positive");

    const auto name_len = syscape::filesystem::max_component_length("/");
    expect(name_len.has_value(), "max component length query must succeed");

    const auto path_len = syscape::filesystem::max_path_length("/");
    expect(path_len.has_value(), "max path length query must succeed");

    const auto vol_id = syscape::filesystem::volume_id("/");
    expect(vol_id && !vol_id->empty(), "volume ID must be nonempty");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
