#ifndef SYSCAPE_DETAIL_IPC_NETBSD_HPP
#define SYSCAPE_DETAIL_IPC_NETBSD_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>

#include <syscape/ipc.hpp>
#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace ipc_backend {

inline result<std::uint64_t> sysctl_ipc_by_name(const char* name) {
    std::size_t sz = 0U;
    if (::sysctlbyname(name, nullptr, &sz, nullptr, 0U) != 0) {
        if (errno == ENOENT) {
            return fail(errc::not_supported);
        }
        if (errno == EACCES || errno == EPERM) {
            return fail(errc::permission_denied);
        }
        return fail(std::error_code(errno, std::generic_category()));
    }
    if (sz == sizeof(int)) {
        int v = 0;
        std::size_t actual_sz = sizeof(v);
        if (::sysctlbyname(name, &v, &actual_sz, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (actual_sz != sizeof(int) || v < 0) {
            return fail(errc::malformed_data);
        }
        return static_cast<std::uint64_t>(v);
    }
    if (sz == sizeof(unsigned long)) {
        unsigned long v = 0;
        std::size_t actual_sz = sizeof(v);
        if (::sysctlbyname(name, &v, &actual_sz, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (actual_sz != sizeof(unsigned long)) {
            return fail(errc::malformed_data);
        }
        return static_cast<std::uint64_t>(v);
    }
    if (sz == sizeof(std::uint64_t)) {
        std::uint64_t v = 0;
        std::size_t actual_sz = sizeof(v);
        if (::sysctlbyname(name, &v, &actual_sz, nullptr, 0U) != 0) {
            if (errno == EACCES || errno == EPERM) {
                return fail(errc::permission_denied);
            }
            return fail(std::error_code(errno, std::generic_category()));
        }
        if (actual_sz != sizeof(std::uint64_t)) {
            return fail(errc::malformed_data);
        }
        return v;
    }
    return fail(errc::malformed_data);
}

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
    ::syscape::ipc::ipc_limits lim;
    std::size_t supported_count = 0;

    auto shmmax = sysctl_ipc_by_name("kern.ipc.shmmax");
    if (!shmmax && shmmax.error() != errc::not_supported) {
        return fail(shmmax.error());
    }
    if (shmmax) {
        lim.max_shared_memory_segment_bytes = *shmmax;
        ++supported_count;
    }

    auto shmmaxpgs = sysctl_ipc_by_name("kern.ipc.shmmaxpgs");
    if (!shmmaxpgs && shmmaxpgs.error() != errc::not_supported) {
        return fail(shmmaxpgs.error());
    }
    if (shmmaxpgs) {
        lim.max_total_shared_memory_pages = *shmmaxpgs;
        ++supported_count;
    }

    auto shmmni = sysctl_ipc_by_name("kern.ipc.shmmni");
    if (!shmmni && shmmni.error() != errc::not_supported) {
        return fail(shmmni.error());
    }
    if (shmmni) {
        lim.max_shared_memory_segments_system = *shmmni;
        ++supported_count;
    }

    auto semmns = sysctl_ipc_by_name("kern.ipc.semmns");
    if (!semmns && semmns.error() != errc::not_supported) {
        return fail(semmns.error());
    }
    if (semmns) {
        lim.max_semaphores_system = *semmns;
        ++supported_count;
    }

    auto semmsl = sysctl_ipc_by_name("kern.ipc.semmsl");
    if (!semmsl && semmsl.error() != errc::not_supported) {
        return fail(semmsl.error());
    }
    if (semmsl) {
        if (*semmsl > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        lim.max_semaphores_per_set = static_cast<std::uint32_t>(*semmsl);
        ++supported_count;
    }

    auto msgmax = sysctl_ipc_by_name("kern.ipc.msgmax");
    if (!msgmax && msgmax.error() != errc::not_supported) {
        return fail(msgmax.error());
    }
    if (msgmax) {
        lim.max_message_bytes = *msgmax;
        ++supported_count;
    }

    auto msgmnb = sysctl_ipc_by_name("kern.ipc.msgmnb");
    if (!msgmnb && msgmnb.error() != errc::not_supported) {
        return fail(msgmnb.error());
    }
    if (msgmnb) {
        lim.default_message_queue_bytes = *msgmnb;
        ++supported_count;
    }

    auto msgmni = sysctl_ipc_by_name("kern.ipc.msgmni");
    if (!msgmni && msgmni.error() != errc::not_supported) {
        return fail(msgmni.error());
    }
    if (msgmni) {
        if (*msgmni > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        lim.max_message_queues_system = static_cast<std::uint32_t>(*msgmni);
        ++supported_count;
    }

    if (supported_count == 0) {
        return fail(errc::not_supported);
    }
    return lim;
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif
