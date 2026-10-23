#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::ecos,
              "eCos must select ecos");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::rtos,
              "eCos must select the rtos execution environment");

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::target_operating_system()),
                       "ecos") == 0);
    return 0;
}
