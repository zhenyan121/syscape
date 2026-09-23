#include <syscape/execution_environment.hpp>

static_assert(syscape::target_operating_system() ==
                  syscape::operating_system::cygwin,
              "Cygwin must select cygwin operating system");
static_assert(syscape::target_execution_environment() ==
                  syscape::execution_environment::compatibility,
              "Cygwin must select compatibility execution environment");
static_assert(syscape::target_compatibility_environment() ==
                  syscape::compatibility_environment::cygwin,
              "Cygwin must select cygwin compatibility environment");

int main() {
    return 0;
}
