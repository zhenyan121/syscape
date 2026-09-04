#ifndef SYSCAPE_DETAIL_HAIKU_ERROR_HPP
#define SYSCAPE_DETAIL_HAIKU_ERROR_HPP

#include <system_error>

#if defined(__has_include)
#if __has_include(<OS.h>)
#include <OS.h>
#endif
#if __has_include(<Errors.h>)
#include <Errors.h>
#endif
#endif

#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace haiku_error {

#if defined(__HAIKU__)
inline std::error_code make_haiku_error(status_t status) {
    if (status == B_OK) {
        return {};
    }
#if defined(B_PERMISSION_DENIED) && defined(B_NOT_ALLOWED)
    if (status == B_PERMISSION_DENIED || status == B_NOT_ALLOWED) {
        return make_error_code(errc::permission_denied);
    }
#endif
#if defined(B_ENTRY_NOT_FOUND) && defined(B_NAME_NOT_FOUND) &&                 \
    defined(B_FILE_NOT_FOUND)
    if (status == B_ENTRY_NOT_FOUND || status == B_NAME_NOT_FOUND ||
        status == B_FILE_NOT_FOUND) {
        return make_error_code(errc::not_found);
    }
#endif
#if defined(B_BAD_TEAM_ID) && defined(B_BAD_THREAD_ID)
    if (status == B_BAD_TEAM_ID || status == B_BAD_THREAD_ID) {
        return make_error_code(errc::not_found);
    }
#endif
#if defined(B_UNSUPPORTED)
    if (status == B_UNSUPPORTED) {
        return make_error_code(errc::not_supported);
    }
#endif
#if defined(B_NO_MEMORY)
    if (status == B_NO_MEMORY) {
        return make_error_code(errc::resource_exhausted);
    }
#endif
#if defined(B_BUSY) && defined(B_TIMED_OUT)
    if (status == B_BUSY || status == B_TIMED_OUT) {
        return make_error_code(errc::temporarily_unavailable);
    }
#endif
#if defined(B_IO_ERROR)
    if (status == B_IO_ERROR) {
        return make_error_code(errc::io_error);
    }
#endif
#if defined(B_BAD_VALUE)
    if (status == B_BAD_VALUE) {
        return make_error_code(errc::invalid_argument);
    }
#endif
    return std::error_code(static_cast<int>(status), std::generic_category());
}

inline bool is_iteration_end(status_t status) noexcept {
#if defined(B_BAD_VALUE)
    if (status == B_BAD_VALUE) {
        return true;
    }
#endif
#if defined(B_ENTRY_NOT_FOUND)
    if (status == B_ENTRY_NOT_FOUND) {
        return true;
    }
#endif
#if defined(B_NAME_NOT_FOUND)
    if (status == B_NAME_NOT_FOUND) {
        return true;
    }
#endif
    (void)status;
    return false;
}
#endif

} // namespace haiku_error
} // namespace detail
} // namespace syscape

#endif
