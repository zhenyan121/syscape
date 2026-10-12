#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::nucleus,
              "Nucleus must select nucleus");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "Nucleus must select the rtos execution environment");

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::target_operating_system()),
                       "nucleus") == 0);
    return 0;
}
