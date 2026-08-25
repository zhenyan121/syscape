#include <cassert>
#include <syscape/process_list.hpp>

int main() {
    const auto list = syscape::process_list::processes();
    assert(!list);
    assert(list.error() == syscape::errc::not_supported);

    const auto count = syscape::process_list::process_count();
    assert(!count);
    assert(count.error() == syscape::errc::not_supported);

    const auto single = syscape::process_list::find_process(1);
    assert(!single);
    assert(single.error() == syscape::errc::not_supported);

    const auto by_name = syscape::process_list::find_processes_by_name("init");
    assert(!by_name);
    assert(by_name.error() == syscape::errc::not_supported);

    return 0;
}
