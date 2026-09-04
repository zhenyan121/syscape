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
           "mounts query must report not_supported on Haiku");

    const auto sp = syscape::filesystem::space("/boot");
    expect(sp.has_value() || sp.error() == syscape::errc::not_found ||
               sp.error() == syscape::errc::not_supported,
           "space query must succeed or report expected error");

    const auto comp = syscape::filesystem::max_component_length("/boot");
    expect(comp.has_value() || comp.error() == syscape::errc::not_supported,
           "max component length must succeed or report not_supported");

    const auto vid = syscape::filesystem::volume_id("/boot");
    expect(vid.has_value() || vid.error() == syscape::errc::not_supported,
           "volume_id query must succeed or report not_supported");
    if (vid) {
        expect(vid->size() == 24,
               "volume_id must be 24 hex characters preserving 32-bit dev_t "
               "and 64-bit root ino_t");
    }

    const auto bad_vid =
        syscape::filesystem::volume_id("/non_existent_path_syscape_99999");
    expect(!bad_vid && bad_vid.error() == syscape::errc::not_found,
           "volume_id query for nonexistent path must report not_found");
}

} // namespace

int main() {
    test_filesystem_queries();
    return failures == 0 ? 0 : 1;
}
