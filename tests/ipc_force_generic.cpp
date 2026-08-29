#include <cstdint>
#include <system_error>

#include <syscape/ipc.hpp>

template <typename T>
bool unsupported(const syscape::result<T>& value) {
    return !value && value.error() == std::errc::operation_not_supported;
}

int main() {
    return unsupported(syscape::ipc::shared_memory_segments()) &&
                   unsupported(syscape::ipc::message_queues()) &&
                   unsupported(syscape::ipc::semaphore_sets()) &&
                   unsupported(syscape::ipc::local_sockets()) &&
                   unsupported(syscape::ipc::limits())
               ? 0
               : 1;
}
