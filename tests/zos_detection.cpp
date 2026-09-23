#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::zos,
              "z/OS must select zos operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::hosted,
              "z/OS must select hosted execution environment");

int main() {
    return 0;
}
