#include <syscape/board.hpp>
#include <syscape/board.hpp>

int main() {
    const syscape::board_family family = syscape::target_board_family();
    const char* const name = syscape::board_family_name(family);
    if (name == nullptr) {
        return 1;
    }
    const syscape::board_info binfo = syscape::board::info();
    return binfo.family == family ? 0 : 2;
}
