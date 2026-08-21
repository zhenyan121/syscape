#include <syscape/toolchain.hpp>

static_assert(syscape::target_cpp_version() >= 201103L,
              "The core target must provide at least C++11");
static_assert(syscape::target_cpp_version() < 201703L,
              "The core target must not force C++17");

int main() {
    return 0;
}
