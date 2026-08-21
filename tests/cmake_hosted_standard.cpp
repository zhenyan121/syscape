#include <syscape/result.hpp>
#include <syscape/toolchain.hpp>

static_assert(syscape::target_cpp_version() >= 201703L,
              "The hosted target must propagate C++17");

int main() {
    const syscape::result<int> value(17);
    return value && *value == 17 ? 0 : 1;
}
