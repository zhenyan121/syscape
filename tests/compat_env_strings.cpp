#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

int main() {
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::cygwin),
                       "cygwin") == 0);
    assert(std::strcmp(syscape::compatibility_environment_name(
                           syscape::compatibility_environment::none),
                       "none") == 0);
    assert(std::strcmp(syscape::compatibility_environment_name(
                           syscape::compatibility_environment::unknown),
                       "unknown") == 0);
    assert(std::strcmp(syscape::compatibility_environment_name(
                           syscape::compatibility_environment::cygwin),
                       "cygwin") == 0);
    assert(std::strcmp(syscape::compatibility_environment_name(
                           syscape::compatibility_environment::mingw),
                       "mingw") == 0);
    assert(std::strcmp(syscape::compatibility_environment_name(
                           syscape::compatibility_environment::msys2),
                       "msys2") == 0);
    return 0;
}
