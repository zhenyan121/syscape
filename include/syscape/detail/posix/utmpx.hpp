#ifndef SYSCAPE_DETAIL_POSIX_UTMPX_HPP
#define SYSCAPE_DETAIL_POSIX_UTMPX_HPP

#include <mutex>

namespace syscape {
namespace detail {
namespace posix_utmpx {

/// Process-wide mutex protecting non-thread-safe POSIX utmpx operations
/// (setutxent, getutxent, endutxent) across all querying modules.
inline std::mutex& mutex() {
    static std::mutex instance;
    return instance;
}

} // namespace posix_utmpx
} // namespace detail
} // namespace syscape

#endif
