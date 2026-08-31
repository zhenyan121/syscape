#ifndef SYSCAPE_DETAIL_IPC_DRAGONFLY_HPP
#define SYSCAPE_DETAIL_IPC_DRAGONFLY_HPP

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <system_error>
#include <vector>

#include <sys/types.h>
#include <sys/sysctl.h>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace ipc_backend {

namespace dragonfly_impl {

inline ::syscape::ipc::ipc_limits parse_dragonfly_sysv_ipc_limits(
    std::optional<std::uint64_t> shmmax, std::optional<std::uint64_t> shmall,
    std::optional<std::uint64_t> shmmni, std::optional<std::uint64_t> msgmax,
    std::optional<std::uint64_t> msgmnb, std::optional<std::uint64_t> msgmni,
    std::optional<std::uint64_t> semmns,
    std::optional<std::uint32_t> semmsl) noexcept {
    ::syscape::ipc::ipc_limits lim;
    lim.max_shared_memory_segment_bytes = shmmax;
    lim.max_total_shared_memory_pages = shmall;
    lim.max_shared_memory_segments_system = shmmni;
    lim.max_message_bytes = msgmax;
    lim.default_message_queue_bytes = msgmnb;
    lim.max_message_queues_system = msgmni;
    lim.max_semaphores_system = semmns;
    lim.max_semaphores_per_set = semmsl;
    return lim;
}

inline result<std::optional<std::uint64_t>>
query_sysctl_uint64(const char* name) {
    std::size_t size = 0;
    errno = 0;
    if (::sysctlbyname(name, nullptr, &size, nullptr, 0) != 0) {
        const int err = errno;
        if (err == ENOENT) {
            return std::optional<std::uint64_t>(std::nullopt);
        }
        if (err == EPERM || err == EACCES) {
            return fail(errc::permission_denied);
        }
        return fail(
            std::error_code(err != 0 ? err : EIO, std::generic_category()));
    }

    if (size == sizeof(int)) {
        int val = 0;
        if (::sysctlbyname(name, &val, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT)
                return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES)
                return fail(errc::permission_denied);
            return fail(
                std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        if (size != sizeof(val) || val < 0) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint64_t>(static_cast<std::uint64_t>(val));
    }

    if (size == sizeof(std::uint32_t)) {
        std::uint32_t val32 = 0;
        if (::sysctlbyname(name, &val32, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT)
                return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES)
                return fail(errc::permission_denied);
            return fail(
                std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        if (size != sizeof(val32)) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint64_t>(static_cast<std::uint64_t>(val32));
    }

    if (size == sizeof(std::uint64_t)) {
        std::uint64_t val64 = 0;
        if (::sysctlbyname(name, &val64, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT)
                return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES)
                return fail(errc::permission_denied);
            return fail(
                std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        if (size != sizeof(val64)) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint64_t>(val64);
    }

    if (size == sizeof(unsigned long)) {
        unsigned long val = 0;
        if (::sysctlbyname(name, &val, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT)
                return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES)
                return fail(errc::permission_denied);
            return fail(
                std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        if (size != sizeof(val)) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint64_t>(static_cast<std::uint64_t>(val));
    }

    return fail(errc::malformed_data);
}

} // namespace dragonfly_impl

inline result<::syscape::ipc::ipc_limits> system_limits() {
    auto shmmax = dragonfly_impl::query_sysctl_uint64("kern.ipc.shmmax");
    if (!shmmax)
        return fail(shmmax.error());

    auto shmall = dragonfly_impl::query_sysctl_uint64("kern.ipc.shmall");
    if (!shmall)
        return fail(shmall.error());

    auto shmmni = dragonfly_impl::query_sysctl_uint64("kern.ipc.shmmni");
    if (!shmmni)
        return fail(shmmni.error());

    auto msgmax = dragonfly_impl::query_sysctl_uint64("kern.ipc.msgmax");
    if (!msgmax)
        return fail(msgmax.error());

    auto msgmnb = dragonfly_impl::query_sysctl_uint64("kern.ipc.msgmnb");
    if (!msgmnb)
        return fail(msgmnb.error());

    auto msgmni = dragonfly_impl::query_sysctl_uint64("kern.ipc.msgmni");
    if (!msgmni)
        return fail(msgmni.error());

    auto semmns = dragonfly_impl::query_sysctl_uint64("kern.ipc.semmns");
    if (!semmns)
        return fail(semmns.error());

    auto semmsl_64 = dragonfly_impl::query_sysctl_uint64("kern.ipc.semmsl");
    if (!semmsl_64)
        return fail(semmsl_64.error());

    std::optional<std::uint32_t> semmsl;
    if (*semmsl_64) {
        if (**semmsl_64 > (std::numeric_limits<std::uint32_t>::max)()) {
            return fail(errc::value_too_large);
        }
        semmsl = static_cast<std::uint32_t>(**semmsl_64);
    }

    if (!*shmmax && !*shmall && !*shmmni && !*msgmax && !*msgmnb && !*msgmni &&
        !*semmns && !semmsl) {
        return fail(errc::not_supported);
    }

    return dragonfly_impl::parse_dragonfly_sysv_ipc_limits(
        *shmmax, *shmall, *shmmni, *msgmax, *msgmnb, *msgmni, *semmns, semmsl);
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

inline result<std::vector<::syscape::ipc::unix_domain_socket>>
unix_domain_sockets() {
    return fail(errc::not_supported);
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif
