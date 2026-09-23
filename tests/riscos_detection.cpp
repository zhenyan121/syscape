#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::riscos,
              "RISC OS must select riscos operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "RISC OS must select rtos execution environment");

int main() {
    return 0;
}
