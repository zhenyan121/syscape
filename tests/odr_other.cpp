#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

const std::error_category* other_error_category() {
    return &syscape::error_category();
}

syscape::architecture other_architecture() {
    return syscape::target_architecture();
}
