#include <syscape/error.hpp>
#include <syscape/error.hpp>

int main() {
    const std::error_code error = syscape::errc::not_supported;
    return error.category() == syscape::error_category() && error ? 0 : 1;
}
