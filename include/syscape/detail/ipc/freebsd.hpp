#ifndef SYSCAPE_DETAIL_IPC_FREEBSD_HPP
#define SYSCAPE_DETAIL_IPC_FREEBSD_HPP

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

namespace freebsd_impl {

inline ::syscape::ipc::ipc_limits parse_freebsd_sysv_ipc_limits(
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

    if (size == sizeof(long)) {
        long val_l = 0;
        if (::sysctlbyname(name, &val_l, &size, nullptr, 0) != 0) {
            const int err = errno;
            if (err == ENOENT)
                return std::optional<std::uint64_t>(std::nullopt);
            if (err == EPERM || err == EACCES)
                return fail(errc::permission_denied);
            return fail(
                std::error_code(err != 0 ? err : EIO, std::generic_category()));
        }
        if (size != sizeof(val_l) || val_l < 0) {
            return fail(errc::malformed_data);
        }
        return std::optional<std::uint64_t>(static_cast<std::uint64_t>(val_l));
    }

    return fail(errc::malformed_data);
}

inline result<std::optional<std::uint32_t>>
query_sysctl_uint32(const char* name) {
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

} // namespace freebsd_impl

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
    auto shmmax_res = freebsd_impl::query_sysctl_uint64("kern.ipc.shmmax");
    if (!shmmax_res.has_value()) {
        return fail(shmmax_res.error());
    }
    auto shmall_res = freebsd_impl::query_sysctl_uint64("kern.ipc.shmall");
    if (!shmall_res.has_value()) {
        return fail(shmall_res.error());
    }
    auto shmmni_res = freebsd_impl::query_sysctl_uint64("kern.ipc.shmmni");
    if (!shmmni_res.has_value()) {
        return fail(shmmni_res.error());
    }
    auto msgmax_res = freebsd_impl::query_sysctl_uint64("kern.ipc.msgmax");
    if (!msgmax_res.has_value()) {
        return fail(msgmax_res.error());
    }
    auto msgmnb_res = freebsd_impl::query_sysctl_uint64("kern.ipc.msgmnb");
    if (!msgmnb_res.has_value()) {
        return fail(msgmnb_res.error());
    }
    auto msgmni_res = freebsd_impl::query_sysctl_uint64("kern.ipc.msgmni");
    if (!msgmni_res.has_value()) {
        return fail(msgmni_res.error());
    }
    auto semmns_res = freebsd_impl::query_sysctl_uint64("kern.ipc.semmns");
    if (!semmns_res.has_value()) {
        return fail(semmns_res.error());
    }
    auto semmsl_res = freebsd_impl::query_sysctl_uint32("kern.ipc.semmsl");
    if (!semmsl_res.has_value()) {
        return fail(semmsl_res.error());
    }

    if (!shmmax_res->has_value() && !shmall_res->has_value() &&
        !shmmni_res->has_value() && !msgmax_res->has_value() &&
        !msgmnb_res->has_value() && !msgmni_res->has_value() &&
        !semmns_res->has_value() && !semmsl_res->has_value()) {
        return fail(errc::not_supported);
    }

    return freebsd_impl::parse_freebsd_sysv_ipc_limits(
        *shmmax_res, *shmall_res, *shmmni_res, *msgmax_res, *msgmnb_res,
        *msgmni_res, *semmns_res, *semmsl_res);
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif
