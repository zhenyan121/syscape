#ifndef SYSCAPE_DETAIL_IPC_WINDOWS_HPP
#define SYSCAPE_DETAIL_IPC_WINDOWS_HPP

#include <vector>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace ipc_backend {

inline result<std::vector<::syscape::ipc::shared_memory_segment>>
shared_memory_segments() {
    // Windows Win32 does not provide System V or POSIX shared memory subsystems.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::message_queue>> message_queues() {
    // Windows Win32 does not provide System V or POSIX message queues.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::semaphore_set>> semaphore_sets() {
    // Windows Win32 does not provide System V or POSIX semaphore sets.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::local_socket>> local_sockets() {
    // Windows does not implement standard POSIX / UNIX domain socket table enumeration.
    return fail(errc::not_supported);
}

inline result<::syscape::ipc::ipc_limits> limits() {
    // Windows does not implement System V IPC subsystems; no IPC limits exist.
    return fail(errc::not_supported);
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_IPC_WINDOWS_HPP
