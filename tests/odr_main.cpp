#include <system_error>

#include <syscape/architecture.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

const std::error_category* other_error_category();
syscape::architecture other_architecture();

int main() {
    if (other_error_category() != &syscape::error_category()) {
        return 1;
    }
    if (other_architecture() != syscape::target_architecture()) {
        return 2;
    }
    const syscape::result<int> value(7);
    return value && *value == 7 ? 0 : 3;
}
