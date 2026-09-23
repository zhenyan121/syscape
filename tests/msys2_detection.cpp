#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::windows,
              "MSYS2 must select windows operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::compatibility,
              "MSYS2 must select compatibility execution environment");
static_assert(syscape::target_compatibility_environment() ==
                  syscape::compatibility_environment::msys2,
              "MSYS2 must select msys2 compatibility environment");

int main() {
    return 0;
}
