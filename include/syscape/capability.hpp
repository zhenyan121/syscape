#ifndef SYSCAPE_CAPABILITY_HPP
#define SYSCAPE_CAPABILITY_HPP

/// @file
/// @brief Allocation-free capability status types shared by Syscape modules.
/// @note Minimum compatibility profile: Freestanding Minimal.
/// @note Minimum language version: C++11; no hosted library is required.

#include <syscape/detail/config.hpp>

namespace syscape {

/// Describes whether a query capability can currently be used.
enum class capability_state {
    unknown,
    unsupported,
    available,
    permission_required,
    temporarily_unavailable
};

/// Stores an allocation-free capability observation.
class capability {
public:
    /// Constructs a capability with an unknown state.
    constexpr capability() noexcept = default;

    /// Constructs a capability from an explicit state.
    constexpr explicit capability(capability_state state) noexcept
        : state_(state) {}

    /// Returns the recorded state.
    constexpr capability_state state() const noexcept { return state_; }

    /// Returns true only when the capability is currently available.
    constexpr bool available() const noexcept {
        return state_ == capability_state::available;
    }

    /// Returns true when the platform recognizes the capability.
    constexpr bool recognized() const noexcept {
        return state_ != capability_state::unknown;
    }

    /// Converts to true only when the capability is currently available.
    constexpr explicit operator bool() const noexcept { return available(); }

private:
    capability_state state_ = capability_state::unknown;
};

/// Returns a stable English name for a capability state.
SYSCAPE_DETAIL_CONSTEXPR14 const char* capability_state_name(
    capability_state value) noexcept {
    switch (value) {
    case capability_state::unsupported: return "unsupported";
    case capability_state::available: return "available";
    case capability_state::permission_required: return "permission-required";
    case capability_state::temporarily_unavailable: return "temporarily-unavailable";
    case capability_state::unknown: return "unknown";
    }
    return "unknown";
}

} // namespace syscape

#endif
