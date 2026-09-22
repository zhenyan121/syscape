#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::mynewt,
              "Apache Mynewt must select mynewt");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Apache Mynewt must select the rtos execution environment");

int main() {
    return 0;
}
