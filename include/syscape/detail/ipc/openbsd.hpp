#ifndef SYSCAPE_DETAIL_IPC_OPENBSD_HPP
#define SYSCAPE_DETAIL_IPC_OPENBSD_HPP

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

inline result<std::uint64_t> sysctl_ipc_val(int m0, int m1, int m2) {
    int mib[3] = {m0, m1, m2};
    std::size_t sz = 0U;
    if (::sysctl(mib, 3, nullptr, &sz, nullptr, 0U) != 0) {
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
        if (::sysctl(mib, 3, &v, &actual_sz, nullptr, 0U) != 0) {
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
    if (sz == sizeof(std::uint64_t)) {
        std::uint64_t v = 0;
        std::size_t actual_sz = sizeof(v);
        if (::sysctl(mib, 3, &v, &actual_sz, nullptr, 0U) != 0) {
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

#if defined(KERN_SHMINFO) && defined(KERN_SHMINFO_SHMMAX)
    auto shmmax = sysctl_ipc_val(CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMMAX);
    if (!shmmax && shmmax.error() != errc::not_supported) {
        return fail(shmmax.error());
    }
    if (shmmax) {
        lim.max_shared_memory_segment_bytes = *shmmax;
    }
#endif

#if defined(KERN_SHMINFO) && defined(KERN_SHMINFO_SHMALL)
    auto shmall = sysctl_ipc_val(CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMALL);
    if (!shmall && shmall.error() != errc::not_supported) {
        return fail(shmall.error());
    }
    if (shmall) {
        lim.max_total_shared_memory_pages = *shmall;
    }
#endif

#if defined(KERN_SHMINFO) && defined(KERN_SHMINFO_SHMMNI)
    auto shmmni = sysctl_ipc_val(CTL_KERN, KERN_SHMINFO, KERN_SHMINFO_SHMMNI);
    if (!shmmni && shmmni.error() != errc::not_supported) {
        return fail(shmmni.error());
    }
    if (shmmni) {
        lim.max_shared_memory_segments_system = *shmmni;
    }
#endif

#if defined(KERN_SEMINFO) && defined(KERN_SEMINFO_SEMMNS)
    auto semmns = sysctl_ipc_val(CTL_KERN, KERN_SEMINFO, KERN_SEMINFO_SEMMNS);
    if (!semmns && semmns.error() != errc::not_supported) {
        return fail(semmns.error());
    }
    if (semmns) {
        lim.max_semaphores_system = *semmns;
    }
#endif

#if defined(KERN_SEMINFO) && defined(KERN_SEMINFO_SEMMSL)
    auto semmsl = sysctl_ipc_val(CTL_KERN, KERN_SEMINFO, KERN_SEMINFO_SEMMSL);
    if (!semmsl && semmsl.error() != errc::not_supported) {
        return fail(semmsl.error());
    }
    if (semmsl) {
        lim.max_semaphores_per_set = static_cast<std::uint32_t>(*semmsl);
    }
#endif

    return lim;
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif
