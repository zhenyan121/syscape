#include <cstdint>
#include <string>
#include <vector>

#include <syscape/filesystem.hpp>
#include <syscape/filesystem.hpp>

int main() {
    const syscape::result<std::vector<syscape::filesystem::mount_entry>>
        mounted = syscape::filesystem::mounts();
    const syscape::result<syscape::filesystem::space_info> capacity =
        syscape::filesystem::space("/");

    static_cast<void>(mounted);
    return capacity && capacity->block_size_bytes == 0U ? 1 : 0;
}
