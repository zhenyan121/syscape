#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::windows,
              "MinGW must select windows operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::compatibility,
              "MinGW must select compatibility execution environment");
static_assert(syscape::target_compatibility_environment() ==
                  syscape::compatibility_environment::mingw,
              "MinGW must select mingw compatibility environment");

int main() {
    return 0;
}
