#ifndef SYSCAPE_DETAIL_IPC_OPENHARMONY_HPP
#define SYSCAPE_DETAIL_IPC_OPENHARMONY_HPP

#include <vector>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace ipc_backend {

inline result<std::vector<::syscape::ipc::shared_memory_segment>>
shared_memory_segments() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::message_queue>> message_queues() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::semaphore_set>> semaphore_sets() {
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::local_socket>> local_sockets() {
    return fail(errc::not_supported);
}

inline result<::syscape::ipc::ipc_limits> limits() {
    return fail(errc::not_supported);
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif
