#ifndef SYSCAPE_DETAIL_IPC_MACOS_HPP
#define SYSCAPE_DETAIL_IPC_MACOS_HPP

#include <cstdint>
#include <optional>
#include <system_error>
#include <vector>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

#if defined(__APPLE__) && defined(__MACH__)
#include <cerrno>
#include <limits>
#include <sys/sysctl.h>
#endif

namespace syscape {
namespace detail {
namespace ipc_backend {

namespace macos_impl {

inline ::syscape::ipc::ipc_limits parse_macos_sysv_ipc_limits(
    std::optional<std::uint64_t> shmmax,
    std::optional<std::uint64_t> shmall,
    std::optional<std::uint64_t> shmmni,
    std::optional<std::uint64_t> msgmax,
    std::optional<std::uint64_t> msgmnb,
    std::optional<std::uint64_t> msgmni,
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

#if defined(__APPLE__) && defined(__MACH__)
inline result<std::optional<std::uint64_t>> query_sysctl_uint64(const char* name) {
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
        return fail(std::error_code(err != 0 ? err : EIO, std::generic_category()));
    }

    if (size == sizeof(std::uint32_t)) {
        std::uint32_t val32 = 0;
        if (::sysctlbyname(name, &val32, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT) return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES) return fail(errc::permission_denied);
            return fail(std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        return std::optional<std::uint64_t>(static_cast<std::uint64_t>(val32));
    }

    if (size == sizeof(std::uint64_t)) {
        std::uint64_t val64 = 0;
        if (::sysctlbyname(name, &val64, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT) return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES) return fail(errc::permission_denied);
            return fail(std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        return std::optional<std::uint64_t>(val64);
    }

    return fail(errc::malformed_data);
}

inline result<std::optional<std::uint32_t>> query_sysctl_uint32(const char* name) {
    const auto res = query_sysctl_uint64(name);
    if (!res.has_value()) {
        return fail(res.error());
    }
    if (!res->has_value()) {
        return std::optional<std::uint32_t>(std::nullopt);
    }
    if (**res > (std::numeric_limits<std::uint32_t>::max)()) {
        return fail(errc::value_too_large);
    }
    return std::optional<std::uint32_t>(static_cast<std::uint32_t>(**res));
}
#endif

} // namespace macos_impl

inline result<std::vector<::syscape::ipc::shared_memory_segment>>
shared_memory_segments() {
    // POSIX / System V shared memory segment enumeration requires elevated entitlements
    // or kernel introspection that is unavailable via unprivileged public macOS APIs.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::message_queue>> message_queues() {
    // Message queue enumeration is not exposed through unprivileged public macOS APIs.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::semaphore_set>> semaphore_sets() {
    // Semaphore set enumeration is not exposed through unprivileged public macOS APIs.
    return fail(errc::not_supported);
}

inline result<std::vector<::syscape::ipc::local_socket>> local_sockets() {
    // Public libproc socket records do not expose the kernel reference count
    // required by the portable local_socket contract.
    return fail(errc::not_supported);
}

inline result<::syscape::ipc::ipc_limits> limits() {
#if defined(__APPLE__) && defined(__MACH__)
    auto shmmax_res = macos_impl::query_sysctl_uint64("kern.sysv.shmmax");
    if (!shmmax_res.has_value()) {
        return fail(shmmax_res.error());
    }
    auto shmall_res = macos_impl::query_sysctl_uint64("kern.sysv.shmall");
    if (!shmall_res.has_value()) {
        return fail(shmall_res.error());
    }
    auto shmmni_res = macos_impl::query_sysctl_uint64("kern.sysv.shmmni");
    if (!shmmni_res.has_value()) {
        return fail(shmmni_res.error());
    }
    auto msgmax_res = macos_impl::query_sysctl_uint64("kern.sysv.msgmax");
    if (!msgmax_res.has_value()) {
        return fail(msgmax_res.error());
    }
    auto msgmnb_res = macos_impl::query_sysctl_uint64("kern.sysv.msgmnb");
    if (!msgmnb_res.has_value()) {
        return fail(msgmnb_res.error());
    }
    auto msgmni_res = macos_impl::query_sysctl_uint64("kern.sysv.msgmni");
    if (!msgmni_res.has_value()) {
        return fail(msgmni_res.error());
    }
    auto semmns_res = macos_impl::query_sysctl_uint64("kern.sysv.semmns");
    if (!semmns_res.has_value()) {
        return fail(semmns_res.error());
    }
    auto semmsl_res = macos_impl::query_sysctl_uint32("kern.sysv.semmsl");
    if (!semmsl_res.has_value()) {
        return fail(semmsl_res.error());
    }

    if (!shmmax_res->has_value() && !shmall_res->has_value() &&
        !shmmni_res->has_value() && !msgmax_res->has_value() &&
        !msgmnb_res->has_value() && !msgmni_res->has_value() &&
        !semmns_res->has_value() && !semmsl_res->has_value()) {
        return fail(errc::not_supported);
    }

    return macos_impl::parse_macos_sysv_ipc_limits(
        *shmmax_res,
        *shmall_res,
        *shmmni_res,
        *msgmax_res,
        *msgmnb_res,
        *msgmni_res,
        *semmns_res,
        *semmsl_res);
#else
    return fail(errc::not_supported);
#endif
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_IPC_MACOS_HPP
