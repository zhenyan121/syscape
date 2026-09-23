#include <cassert>
#include <cstring>
#include <syscape/execution_environment.hpp>

int main() {
    assert(std::strcmp(
               syscape::operating_system_name(syscape::operating_system::dos),
               "dos") == 0);
    assert(std::strcmp(
               syscape::operating_system_name(syscape::operating_system::os2),
               "os2") == 0);
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::amigaos),
                       "amigaos") == 0);
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::riscos),
                       "riscos") == 0);
    assert(std::strcmp(syscape::operating_system_name(
                           syscape::operating_system::openvms),
                       "openvms") == 0);
    assert(std::strcmp(
               syscape::operating_system_name(syscape::operating_system::zos),
               "zos") == 0);
    return 0;
}
