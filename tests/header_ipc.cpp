#include <syscape/ipc.hpp>
#include <syscape/ipc.hpp>

#include <cstdint>
#include <system_error>

namespace {

template <typename Query>
bool honest(const Query& query) {
    try {
        const auto value = query();
        static_cast<void>(value);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int main() {
    return honest(syscape::ipc::shared_memory_segments) &&
                   honest(syscape::ipc::message_queues) &&
                   honest(syscape::ipc::semaphore_sets) &&
                   honest(syscape::ipc::local_sockets) &&
                   honest(syscape::ipc::limits)
               ? 0
               : 1;
}
