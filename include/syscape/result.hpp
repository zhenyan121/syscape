#ifndef SYSCAPE_RESULT_HPP
#define SYSCAPE_RESULT_HPP

/// @file
/// @brief A C++17 value-or-error type for Hosted Full queries.
/// @note Minimum compatibility profile: Hosted Full with C++17.

#include <syscape/detail/config.hpp>

#if SYSCAPE_DETAIL_CPLUSPLUS < 201703L
#error "syscape/result.hpp requires C++17 or later"
#endif

#include <stdexcept>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>

#include <syscape/error.hpp>

namespace syscape {

/// Wraps an error used to construct a failed result.
class unexpected {
public:
    /// Constructs a failure from a nonzero standard error code.
    explicit unexpected(std::error_code error) noexcept : error_(error) {}

    /// Returns the wrapped error code.
    const std::error_code& error() const noexcept { return error_; }

private:
    std::error_code error_;
};

/// Creates a failure wrapper from a standard error code.
inline unexpected fail(std::error_code error) noexcept {
    return unexpected(error);
}

/// Creates a failure wrapper from a portable Syscape error.
inline unexpected fail(errc error) noexcept {
    return unexpected(make_error_code(error));
}

/// Reports an attempt to access the value of a failed result.
class bad_result_access : public std::logic_error {
public:
    /// Constructs the exception from the failed result's error code.
    explicit bad_result_access(std::error_code error)
        : std::logic_error(error ? error.message() : "result has no value"),
          error_(error) {}

    /// Returns the error stored by the failed result.
    const std::error_code& error() const noexcept { return error_; }

private:
    std::error_code error_;
};

/// Stores either a value of T or a standard error code.
template <typename T>
class result {
    static_assert(!std::is_reference<T>::value, "result<T> cannot store a reference");
    static_assert(!std::is_void<T>::value, "use result<void> for a void value");

public:
    using value_type = T;
    using error_type = std::error_code;

    /// Constructs a successful result by default-constructing T.
    template <typename U = T,
              typename std::enable_if<std::is_default_constructible<U>::value,
                                      int>::type = 0>
    constexpr result() noexcept(std::is_nothrow_default_constructible<T>::value)
        : storage_(std::in_place_index<0>) {}

    /// Constructs a successful result by copying a value.
    constexpr result(const T& value) : storage_(std::in_place_index<0>, value) {}

    /// Constructs a successful result by moving a value.
    constexpr result(T&& value) noexcept(std::is_nothrow_move_constructible<T>::value)
        : storage_(std::in_place_index<0>, std::move(value)) {}

    /// Constructs a failed result by copying an unexpected value.
    constexpr result(const unexpected& failure)
        : storage_(std::in_place_index<1>, failure.error()) {}

    /// Constructs a failed result by moving an unexpected value.
    constexpr result(unexpected&& failure)
        : storage_(std::in_place_index<1>, failure.error()) {}

    /// Returns true when this object contains a value.
    constexpr bool has_value() const noexcept { return storage_.index() == 0U; }

    /// Converts to true when this object contains a value.
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /// Returns the stored value or throws bad_result_access on failure.
    T& value() & {
        ensure_value();
        return std::get<0>(storage_);
    }

    /// Returns the stored value or throws bad_result_access on failure.
    const T& value() const& {
        ensure_value();
        return std::get<0>(storage_);
    }

    /// Returns the stored value or throws bad_result_access on failure.
    T&& value() && {
        ensure_value();
        return std::get<0>(std::move(storage_));
    }

    /// Returns the stored value or throws bad_result_access on failure.
    const T&& value() const&& {
        ensure_value();
        return std::get<0>(std::move(storage_));
    }

    /// Returns the stored error, or a success code when this object has a value.
    const std::error_code& error() const noexcept {
        if (has_value()) {
            static const std::error_code success;
            return success;
        }
        return std::get<1>(storage_);
    }

    /// Returns the stored value. The behavior is undefined on failure.
    T& operator*() & noexcept { return *std::get_if<0>(&storage_); }

    /// Returns the stored value. The behavior is undefined on failure.
    const T& operator*() const& noexcept { return *std::get_if<0>(&storage_); }

    /// Returns the stored value. The behavior is undefined on failure.
    T&& operator*() && noexcept {
        return std::move(*std::get_if<0>(&storage_));
    }

    /// Returns a pointer to the stored value. The behavior is undefined on failure.
    T* operator->() noexcept { return std::get_if<0>(&storage_); }

    /// Returns a pointer to the stored value. The behavior is undefined on failure.
    const T* operator->() const noexcept { return std::get_if<0>(&storage_); }

    /// Returns the value or a caller-provided fallback.
    template <typename U>
    T value_or(U&& fallback) const& {
        return has_value() ? std::get<0>(storage_)
                           : static_cast<T>(std::forward<U>(fallback));
    }

    /// Returns the moved value or a caller-provided fallback.
    template <typename U>
    T value_or(U&& fallback) && {
        return has_value() ? std::get<0>(std::move(storage_))
                           : static_cast<T>(std::forward<U>(fallback));
    }

private:
    void ensure_value() const {
        if (!has_value()) {
            throw bad_result_access(std::get<1>(storage_));
        }
    }

    std::variant<T, std::error_code> storage_;
};

/// Specialization of result for operations with no value payload.
template <>
class result<void> {
public:
    using value_type = void;
    using error_type = std::error_code;

    /// Constructs a successful result.
    result() noexcept = default;

    /// Constructs a failed result.
    result(const unexpected& failure) noexcept
        : error_(failure.error()), has_value_(false) {}

    /// Constructs a failed result.
    result(unexpected&& failure) noexcept
        : error_(failure.error()), has_value_(false) {}

    /// Returns true when the operation succeeded.
    constexpr bool has_value() const noexcept { return has_value_; }

    /// Converts to true when the operation succeeded.
    constexpr explicit operator bool() const noexcept { return has_value(); }

    /// Returns normally on success or throws bad_result_access on failure.
    void value() const {
        if (!has_value_) {
            throw bad_result_access(error_);
        }
    }

    /// Returns the stored error, or a success code when the operation succeeded.
    const std::error_code& error() const noexcept { return error_; }

private:
    std::error_code error_;
    bool has_value_ = true;
};

} // namespace syscape

#endif
