#include <cstdint>
#include <system_error>

#include <syscape/storage.hpp>

int main() {
    const syscape::result<std::vector<syscape::storage::drive_entry>> listed_drives =
        syscape::storage::drives();
    if (listed_drives ||
        listed_drives.error() != std::errc::operation_not_supported) {
        return 1;
    }

    const syscape::result<std::vector<syscape::storage::partition_entry>> listed_parts =
        syscape::storage::partitions();
    if (listed_parts ||
        listed_parts.error() != std::errc::operation_not_supported) {
        return 1;
    }

    const syscape::result<std::vector<syscape::storage::partition_entry>> disk_parts =
        syscape::storage::disk_partitions("disk0");
    if (disk_parts ||
        disk_parts.error() != std::errc::operation_not_supported) {
        return 1;
    }

    return 0;
}
