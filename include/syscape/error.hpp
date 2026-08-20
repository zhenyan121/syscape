#ifndef SYSCAPE_ERROR_HPP
#define SYSCAPE_ERROR_HPP

/// @file
/// @brief Portable Hosted Full error codes and standard error integration.
/// @note Minimum compatibility profile: Hosted Full.

#include <string>
#include <system_error>
#include <type_traits>

#include <syscape/detail/config.hpp>

namespace syscape {

/// Identifies portable failures produced by Syscape itself.
enum class errc {
    success = 0,
    unknown = 1,
    not_supported,
    permission_denied,
    not_found,
    temporarily_unavailable,
    malformed_data,
    io_error,
    invalid_encoding,
    value_too_large,
    resource_exhausted,
    invalid_argument
};

namespace detail {

class syscape_error_category final : public std::error_category {
public:
    const char* name() const noexcept override { return "syscape"; }

    std::string message(int value) const override {
        switch (static_cast<errc>(value)) {
        case errc::success: return "success";
        case errc::unknown: return "unknown error";
        case errc::not_supported: return "capability not supported";
        case errc::permission_denied: return "permission denied";
        case errc::not_found: return "information not found";
        case errc::temporarily_unavailable: return "information temporarily unavailable";
        case errc::malformed_data: return "malformed platform data";
        case errc::io_error: return "input or output error";
        case errc::invalid_encoding: return "invalid text encoding";
        case errc::value_too_large: return "value too large";
        case errc::resource_exhausted: return "resource exhausted";
        case errc::invalid_argument: return "invalid argument";
        }
        return "unrecognized syscape error";
    }

    std::error_condition default_error_condition(int value) const noexcept override {
        switch (static_cast<errc>(value)) {
        case errc::success: return {};
        case errc::not_supported:
            return std::make_error_condition(std::errc::operation_not_supported);
        case errc::permission_denied:
            return std::make_error_condition(std::errc::permission_denied);
        case errc::not_found:
            return std::make_error_condition(std::errc::no_such_file_or_directory);
        case errc::temporarily_unavailable:
            return std::make_error_condition(std::errc::resource_unavailable_try_again);
        case errc::malformed_data:
        case errc::invalid_encoding:
            return std::make_error_condition(std::errc::illegal_byte_sequence);
        case errc::io_error:
            return std::make_error_condition(std::errc::io_error);
        case errc::value_too_large:
            return std::make_error_condition(std::errc::value_too_large);
        case errc::resource_exhausted:
            return std::make_error_condition(std::errc::not_enough_memory);
        case errc::invalid_argument:
            return std::make_error_condition(std::errc::invalid_argument);
        case errc::unknown:
            break;
        }
        return std::error_condition(value, *this);
    }
};

} // namespace detail

/// Returns the process-wide error category used by syscape::errc.
inline const std::error_category& error_category() noexcept {
    static const detail::syscape_error_category category;
    return category;
}

/// Creates a standard error code from a portable Syscape error.
inline std::error_code make_error_code(errc value) noexcept {
    return {static_cast<int>(value), error_category()};
}

} // namespace syscape

namespace std {

template <>
struct is_error_code_enum<syscape::errc> : true_type {};

} // namespace std

#endif
