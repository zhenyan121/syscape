#include <syscape/toolchain.hpp>
#include <syscape/toolchain.hpp>

int main() {
    return syscape::target_cpp_version() >= 201703L &&
                   syscape::compiler_name(syscape::target_compiler()) != nullptr
               ? 0
               : 1;
}
