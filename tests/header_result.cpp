#include <syscape/result.hpp>
#include <syscape/result.hpp>

int main() {
    const syscape::result<int> value(42);
    const syscape::result<int> failure =
        syscape::fail(syscape::errc::not_supported);
    return value && *value == 42 && !failure ? 0 : 1;
}
