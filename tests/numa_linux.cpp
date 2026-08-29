#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include <syscape/numa.hpp>
#include <syscape/detail/numa/linux.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_parse_range_list() {
    const auto single = syscape::detail::numa_backend::parse_range_list("0");
    expect(single && single->size() == 1U && (*single)[0] == 0U,
           "Single integer range must parse correctly");

    const auto range = syscape::detail::numa_backend::parse_range_list("0-3");
    expect(range && range->size() == 4U && (*range)[0] == 0U && (*range)[3] == 3U,
           "Hyphenated range 0-3 must expand to 4 elements");

    const auto mixed = syscape::detail::numa_backend::parse_range_list("0,2,4-6");
    expect(mixed && mixed->size() == 5U && (*mixed)[0] == 0U && (*mixed)[1] == 2U &&
               (*mixed)[2] == 4U && (*mixed)[4] == 6U,
           "Mixed comma and hyphenated ranges must parse correctly");

    const auto empty = syscape::detail::numa_backend::parse_range_list("   ");
    expect(!empty && empty.error() == syscape::errc::malformed_data,
           "Empty input must fail as malformed data");

    const auto inverted = syscape::detail::numa_backend::parse_range_list("5-2");
    expect(!inverted && inverted.error() == syscape::errc::malformed_data,
           "Inverted range 5-2 must fail as malformed data");

    const auto garbage = syscape::detail::numa_backend::parse_range_list("abc");
    expect(!garbage && garbage.error() == syscape::errc::malformed_data,
           "Nonnumeric range must fail as malformed data");

    const auto double_dash = syscape::detail::numa_backend::parse_range_list("0-2-4");
    expect(!double_dash && double_dash.error() == syscape::errc::malformed_data,
           "Multiple dashes must fail as malformed data");

    const auto trailing_comma = syscape::detail::numa_backend::parse_range_list("0,");
    expect(!trailing_comma && trailing_comma.error() == syscape::errc::malformed_data,
           "Trailing comma must fail as malformed data");

    const auto leading_comma = syscape::detail::numa_backend::parse_range_list(",0");
    expect(!leading_comma && leading_comma.error() == syscape::errc::malformed_data,
           "Leading comma must fail as malformed data");
}

void test_parse_distance_list() {
    const auto single = syscape::detail::numa_backend::parse_distance_list("10");
    expect(single && single->size() == 1U && (*single)[0] == 10U,
           "Single distance 10 must parse correctly");

    const auto multi =
        syscape::detail::numa_backend::parse_distance_list("10  20   30");
    expect(multi && multi->size() == 3U && (*multi)[0] == 10U && (*multi)[1] == 20U &&
               (*multi)[2] == 30U,
           "Multiple space-separated distances must parse correctly");

    const auto empty = syscape::detail::numa_backend::parse_distance_list("   ");
    expect(empty && empty->empty(), "Empty distance string returns empty vector");

    const auto invalid = syscape::detail::numa_backend::parse_distance_list("10 abc");
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Nonnumeric distance token must fail as malformed data");
}

void test_parse_node_meminfo() {
    const char* sample =
        "Node 0 MemTotal:       32117348 kB\n"
        "Node 0 MemFree:        24822384 kB\n"
        "Node 0 MemUsed:         7294964 kB\n"
        "Node 0 Active:          4081384 kB\n";

    const auto parsed = syscape::detail::numa_backend::parse_node_meminfo(sample);
    expect(parsed.has_value(), "Valid node meminfo block must parse");
    if (parsed) {
        expect(parsed->total_bytes && *parsed->total_bytes == 32117348ULL * 1024ULL,
               "MemTotal parsed in bytes");
        expect(parsed->free_bytes && *parsed->free_bytes == 24822384ULL * 1024ULL,
               "MemFree parsed in bytes");
        expect(parsed->used_bytes && *parsed->used_bytes == 7294964ULL * 1024ULL,
               "MemUsed parsed in bytes");
    }

    const char* invalid_sample = "Node 0 MemTotal:   invalid kB\n";
    const auto invalid_parsed =
        syscape::detail::numa_backend::parse_node_meminfo(invalid_sample);
    expect(!invalid_parsed && invalid_parsed.error() == syscape::errc::malformed_data,
           "Invalid meminfo numeric value must fail as malformed data");
}

void test_numa_node_validation() {
    syscape::numa::numa_node node;
    node.id = 0U;
    node.total_memory_bytes = 1000U;
    node.free_memory_bytes = 500U;
    node.logical_processors = {3U, 1U, 2U, 2U};

    const auto valid = syscape::detail::numa_common::validate_numa_node(node);
    expect(valid.has_value(), "Valid node passes validation");
    if (valid) {
        expect(valid->logical_processors.size() == 3U &&
                   valid->logical_processors[0] == 1U &&
                   valid->logical_processors[1] == 2U &&
                   valid->logical_processors[2] == 3U,
               "Logical processors are sorted and deduplicated");
    }

    syscape::numa::numa_node invalid_node;
    invalid_node.id = 1U;
    invalid_node.total_memory_bytes = 1000U;
    invalid_node.free_memory_bytes = 2000U;
    const auto invalid =
        syscape::detail::numa_common::validate_numa_node(invalid_node);
    expect(!invalid && invalid.error() == syscape::errc::malformed_data,
           "Free memory exceeding total memory must fail validation");

    syscape::numa::numa_node invalid_used_node;
    invalid_used_node.id = 2U;
    invalid_used_node.total_memory_bytes = 1000U;
    invalid_used_node.used_memory_bytes = 1500U;
    const auto invalid_used =
        syscape::detail::numa_common::validate_numa_node(invalid_used_node);
    expect(!invalid_used && invalid_used.error() == syscape::errc::malformed_data,
           "Used memory exceeding total memory must fail validation");
}

bool write_fixture(const std::string& path, const char* value) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << value;
    return output.good();
}

void test_sysfs_availability_and_online_state() {
    char root_template[] = "/tmp/syscape-numa-XXXXXX";
    const char* root_value = ::mkdtemp(root_template);
    expect(root_value != nullptr, "Created a temporary NUMA sysfs fixture");
    if (root_value == nullptr) { return; }

    const std::string root(root_value);
    const std::string missing_root = root + "/missing";
    const auto missing = syscape::detail::numa_backend::read_online_nodes_at(
        missing_root.c_str());
    expect(!missing &&
               missing.error() == std::errc::no_such_file_or_directory,
           "A missing NUMA sysfs source must preserve the native error");

    const std::string node0 = root + "/node0";
    const std::string node1 = root + "/node1";
    expect(::mkdir(node0.c_str(), 0700) == 0 &&
               ::mkdir(node1.c_str(), 0700) == 0,
           "Created synthetic NUMA node directories");
    expect(write_fixture(root + "/online", "0\n") &&
               write_fixture(root + "/possible", "0-1\n"),
           "Wrote distinct online and possible node sets");

    const auto online =
        syscape::detail::numa_backend::read_online_nodes_at(root.c_str());
    expect(online && online->size() == 1U && (*online)[0U] == 0U,
           "Online enumeration must not substitute the possible node set");

    const auto offline_node =
        syscape::detail::numa_backend::node_at(1U, root.c_str());
    expect(offline_node && offline_node->id == 1U &&
               !offline_node->is_online,
           "A possible but offline node must not be reported as online");

    static_cast<void>(::unlink((root + "/online").c_str()));
    static_cast<void>(::unlink((root + "/possible").c_str()));
    static_cast<void>(::rmdir(node0.c_str()));
    static_cast<void>(::rmdir(node1.c_str()));
    static_cast<void>(::rmdir(root.c_str()));
}

void test_live_numa_queries() {
    const auto numa_avail = syscape::numa::is_numa_available();
    expect(numa_avail.has_value(), "is_numa_available() should succeed on Linux");

    const auto count = syscape::numa::node_count();
    expect(count.has_value() && *count >= 1U,
           "node_count() should return at least 1 node");

    const auto nodes = syscape::numa::nodes();
    expect(nodes.has_value() && !nodes->empty(),
           "nodes() should return at least 1 node");
    if (count && nodes) {
        expect(nodes->size() == *count, "nodes() size must match node_count()");

        // Verify ordering
        for (std::size_t i = 1U; i < nodes->size(); ++i) {
            expect((*nodes)[i - 1U].id < (*nodes)[i].id,
                   "Nodes must be sorted in ascending order of ID");
        }
    }

    const auto node0 = syscape::numa::node(0U);
    expect(node0.has_value(), "node(0) must succeed on a running Linux host");
    if (node0) {
        expect(node0->id == 0U, "node(0) id must be 0");
    }

    const auto non_existent = syscape::numa::node(999999U);
    expect(!non_existent && non_existent.error() == syscape::errc::not_found,
           "Non-existent node ID must return not_found");

    const auto thread_node = syscape::numa::current_thread_node();
    expect(thread_node.has_value(),
           "current_thread_node() should succeed on Linux");
    if (thread_node && nodes) {
        bool found_thread_node = false;
        for (const auto& n : *nodes) {
            if (n.id == *thread_node) {
                found_thread_node = true;
                break;
            }
        }
        expect(found_thread_node,
               "current_thread_node() must return an existing node ID");
    }
}

} // namespace

int main() {
    test_parse_range_list();
    test_parse_distance_list();
    test_parse_node_meminfo();
    test_numa_node_validation();
    test_sysfs_availability_and_online_state();
    test_live_numa_queries();
    return failures == 0 ? 0 : 1;
}
