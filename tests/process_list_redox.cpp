#include <iostream>

#include <syscape/process.hpp>
#include <syscape/process_list.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

struct fake_context_reader {
    static std::string mock_data;
    static syscape::result<std::string> read_context_file() {
        if (mock_data.empty()) {
            return syscape::fail(syscape::errc::not_supported);
        }
        return mock_data;
    }
};

std::string fake_context_reader::mock_data;

void test_context_parsing() {
    const char* sample = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        "
                         "PRIVATE SHARED  NAME\n"
                         "1     0     0     KR+   0     all        00:00:01.23 "
                         "100 KB  0 B     init\n"
                         "2     0     0     KS    ?     all        00:00:00.05 "
                         "40 KB   0 B     context2\n"
                         "10    1000  1000  UR+   1     all        00:00:02.00 "
                         "1.25 MB 500 KB  sh\n"
                         "20    1000  1000  UZ    1     all        00:00:00.10 "
                         "10 KB   0 B     zombie_proc\n";

    const auto procs =
        syscape::detail::process_list_backend::parse_sys_context(sample);
    expect(procs.has_value(), "parse_sys_context must succeed on valid sample");
    if (procs) {
        expect(procs->size() == 4U, "must parse 4 processes");
        if (procs->size() >= 4U) {
            expect((*procs)[0].pid == 1U, "first PID must be 1");
            expect(!(*procs)[0].uid.has_value(),
                   "PID 1 UID must be nullopt on Redox");
            expect(!(*procs)[0].gid.has_value(),
                   "PID 1 GID must be nullopt on Redox");
            expect(!(*procs)[0].user_name.has_value(),
                   "PID 1 user_name must be nullopt on Redox");
            expect((*procs)[0].state ==
                       syscape::process_list::process_state::running,
                   "PID 1 state must be running");
            expect(!(*procs)[0].name.has_value(),
                   "PID 1 name must be nullopt on Redox");

            expect((*procs)[1].pid == 2U, "second PID must be 2");
            expect((*procs)[1].state ==
                       syscape::process_list::process_state::sleeping,
                   "PID 2 state must be sleeping");
            expect(!(*procs)[1].name.has_value(),
                   "PID 2 name must be nullopt on Redox");

            expect((*procs)[2].pid == 10U, "third PID must be 10");
            expect(!(*procs)[2].uid.has_value(),
                   "PID 10 UID must be nullopt on Redox");
            expect((*procs)[2].state ==
                       syscape::process_list::process_state::running,
                   "PID 10 state must be running");
            expect(!(*procs)[2].name.has_value(),
                   "PID 10 name must be nullopt on Redox");

            expect((*procs)[3].pid == 20U, "fourth PID must be 20");
            expect((*procs)[3].state ==
                       syscape::process_list::process_state::zombie,
                   "PID 20 state must be zombie");
            expect(!(*procs)[3].name.has_value(),
                   "PID 20 name must be nullopt on Redox");
        }
    }
}

void test_context_threads_deduplication() {
    // Two contexts share PID 2 (multithreaded process): one sleeping, one
    // running. Result must have exactly 2 processes (PID 1 and PID 2), and PID
    // 2 state must be running.
    const char* sample = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        "
                         "PRIVATE SHARED  NAME\n"
                         "1     0     0     KR+   0     all        00:00:01.00 "
                         "100 KB  0 B     init\n"
                         "2     100   100   UB    0     all        00:00:00.10 "
                         "50 KB   0 B     multi_app\n"
                         "2     100   100   UR+   1     all        00:00:00.20 "
                         "50 KB   0 B     multi_app\n";

    const auto procs =
        syscape::detail::process_list_backend::parse_sys_context(sample);
    expect(procs.has_value(),
           "parse_sys_context must succeed with multiple threads");
    if (procs) {
        expect(procs->size() == 2U, "multithreaded contexts must be "
                                    "deduplicated by PID into 2 processes");
        if (procs->size() >= 2U) {
            expect((*procs)[0].pid == 1U, "first PID must be 1");
            expect((*procs)[1].pid == 2U, "second PID must be 2");
            expect((*procs)[1].state ==
                       syscape::process_list::process_state::running,
                   "multithreaded process with active thread must have running "
                   "state");
            expect(!(*procs)[1].name.has_value(),
                   "process name must remain nullopt on Redox");
        }
    }
}

void test_context_long_affinity() {
    // In Redox kernel context.rs, AFFINITY is {:<11}.
    // A long CPU affinity mask (> 11 chars) like FFFFFFFF_FFFFFFFF (17 chars)
    // abuts the TIME field directly without whitespace.
    const char* sample =
        "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE SHARED  "
        "NAME\n"
        "1     0     0     KR+   0     FFFFFFFF_FFFFFFFF00:00:01.00 100 KB  0 "
        "B     server_daemon\n";

    const auto procs =
        syscape::detail::process_list_backend::parse_sys_context(sample);
    expect(procs.has_value(),
           "parse_sys_context must succeed with long CPU affinity mask");
    if (procs) {
        expect(procs->size() == 1U, "must parse 1 process");
        if (!procs->empty()) {
            expect((*procs)[0].pid == 1U, "PID must be 1");
            expect((*procs)[0].state ==
                       syscape::process_list::process_state::running,
                   "state must be running");
            expect(!(*procs)[0].name.has_value(),
                   "name must remain nullopt on Redox");
        }
    }
}

void test_context_malformed_handling() {
    // Invalid PID
    {
        const char* bad = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME       "
                          " PRIVATE SHARED  NAME\n"
                          "XYZ   0     0     KR+   0     all        "
                          "00:00:01.00 100 KB  0 B     init\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(bad);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "invalid PID must report malformed_data");
    }
    // Invalid EUID
    {
        const char* bad = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME       "
                          " PRIVATE SHARED  NAME\n"
                          "1     XYZ   0     KR+   0     all        "
                          "00:00:01.00 100 KB  0 B     init\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(bad);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "invalid EUID must report malformed_data");
    }
    // Missing memory unit
    {
        const char* bad = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME       "
                          " PRIVATE SHARED  NAME\n"
                          "1     0     0     KR+   0     all        "
                          "00:00:01.00 100     0 B     init\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(bad);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "missing memory unit must report malformed_data");
    }
    // Corrupted time format
    {
        const char* bad = "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME       "
                          " PRIVATE SHARED  NAME\n"
                          "1     0     0     KR+   0     all        not_a_time "
                          " 100 KB  0 B     init\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(bad);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "corrupted time must report malformed_data");
    }
    // Invalid STAT values
    {
        const char* bad_stats[] = {
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "1     0     0     UX    0     all        00:00:01.00 100 KB  0 B  "
            "   init\n",
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "1     0     0     UR+x  0     all        00:00:01.00 100 KB  0 B  "
            "   init\n",
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "1     0     0     URx   0     all        00:00:01.00 100 KB  0 B  "
            "   init\n",
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "1     0     0     XK    0     all        00:00:01.00 100 KB  0 B  "
            "   init\n",
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "1     0     0     K     0     all        00:00:01.00 100 KB  0 B  "
            "   init\n",
        };
        for (const char* bad : bad_stats) {
            const auto res =
                syscape::detail::process_list_backend::parse_sys_context(bad);
            expect(!res && res.error() == syscape::errc::malformed_data,
                   "invalid STAT format must report malformed_data");
        }
    }
    // Empty content and whitespace-only content
    {
        const auto res1 =
            syscape::detail::process_list_backend::parse_sys_context("");
        expect(!res1 && res1.error() == syscape::errc::malformed_data,
               "empty content must report malformed_data");

        const auto res2 =
            syscape::detail::process_list_backend::parse_sys_context(
                "   \n\t\n# comment\n");
        expect(!res2 && res2.error() == syscape::errc::malformed_data,
               "whitespace and comments only must report malformed_data");
    }
    // Header-only content
    {
        const char* header_only =
            "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(
                header_only);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "header-only content without processes must report "
               "malformed_data");
    }
    // Corrupted header
    {
        const char* bad_header =
            "PID123 EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE "
            "SHARED  NAME\n"
            "0     0     0     RS    0     all        00:00:05.00 0 B      0 B "
            " "
            "   [kmain]\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(
                bad_header);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "corrupted header must report malformed_data");
    }
    // Missing header (starts with process directly)
    {
        const char* no_header = "0     0     0     RS    0     all        "
                                "00:00:05.00 0 B      0 B  "
                                "   [kmain]\n";
        const auto res =
            syscape::detail::process_list_backend::parse_sys_context(no_header);
        expect(!res && res.error() == syscape::errc::malformed_data,
               "content without header must report malformed_data");
    }
}

void test_context_wide_memory() {
    const char* sample =
        "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE SHARED  "
        "NAME\n"
        "1     0     0     KR+   0     all        00:00:01.00 10000 GB0 B     "
        "init\n"
        "2     0     0     KS    0     all        00:00:00.05 0 B     10000 "
        "GBcontext2\n"
        "3     0     0     UR+   1     all        00:00:02.00 10000 GB10000 "
        "GBsh\n";

    const auto procs =
        syscape::detail::process_list_backend::parse_sys_context(sample);
    expect(procs.has_value(),
           "parse_sys_context must succeed with wide memory fields >= 10000 "
           "GB");
    if (procs) {
        expect(procs->size() == 3U, "must parse 3 processes with wide memory");
        if (procs->size() >= 3U) {
            expect((*procs)[0].pid == 1U, "PID 1 must parse");
            expect((*procs)[1].pid == 2U, "PID 2 must parse");
            expect((*procs)[2].pid == 3U, "PID 3 must parse");
        }
    }
}

void test_context_pid0_and_stat_and_long_time() {
    const char* sample =
        "PID   EUID  EGID  STAT  CPU   AFFINITY   TIME        PRIVATE SHARED  "
        "NAME\n"
        "0     0     0     RS    0     all        00:00:05.00 0 B      0 B     "
        "[kmain]\n"
        "1     0     0     RB    0     all        00:00:01.00 100 KB   0 B     "
        "init\n"
        "2     0     0     UR+   1     all        100:15:30.45500 KB   0 B     "
        "long_runner\n";

    const auto procs =
        syscape::detail::process_list_backend::parse_sys_context(sample);
    expect(procs.has_value(),
           "parse_sys_context must succeed with PID 0, RS/RB stats, and 100+ "
           "hr time");
    if (procs) {
        expect(procs->size() == 3U, "must parse 3 processes");
        if (procs->size() >= 3U) {
            expect((*procs)[0].pid == 0U, "first PID must be 0");
            expect(!(*procs)[0].name.has_value(), "PID 0 name must be nullopt");
            expect((*procs)[0].state ==
                       syscape::process_list::process_state::sleeping,
                   "RS stat must parse as sleeping");

            expect((*procs)[1].pid == 1U, "second PID must be 1");
            expect(!(*procs)[1].name.has_value(), "PID 1 name must be nullopt");
            expect((*procs)[1].state ==
                       syscape::process_list::process_state::sleeping,
                   "RB stat must parse as sleeping");

            expect((*procs)[2].pid == 2U, "third PID must be 2");
            expect(!(*procs)[2].name.has_value(), "PID 2 name must be nullopt");
            expect((*procs)[2].state ==
                       syscape::process_list::process_state::running,
                   "UR+ stat must parse as running");
        }
    }
}

void test_injected_process_list() {
    fake_context_reader::mock_data = "PID   EUID  EGID  STAT  CPU   AFFINITY   "
                                     "TIME        PRIVATE SHARED  NAME\n"
                                     "0     0     0     RS    0     all        "
                                     "00:00:05.00 0 B      0 B     [kmain]\n"
                                     "5     100   100   UR+   0     all        "
                                     "00:00:00.10 50 KB   0 B     daemon\n"
                                     "1     0     0     KR+   0     all        "
                                     "00:00:01.00 100 KB  0 B     init\n";

    const auto procs =
        syscape::detail::process_list_backend::processes<fake_context_reader>();
    expect(procs.has_value(), "injected processes query must succeed");
    if (procs) {
        expect(procs->size() == 3U, "injected processes must return 3 entries");
        if (procs->size() >= 3U) {
            expect((*procs)[0].pid == 0U && (*procs)[1].pid == 1U &&
                       (*procs)[2].pid == 5U,
                   "processes must be naturally sorted by PID including PID 0");
        }
    }

    const auto count = syscape::detail::process_list_backend::process_count<
        fake_context_reader>();
    expect(count.has_value() && *count == 3U,
           "injected process count must be 3");

    const auto p0 = syscape::detail::process_list_backend::find_process<
        fake_context_reader>(0U);
    expect(!p0 && p0.error() == syscape::errc::not_found,
           "find_process(0) must always report not_found");

    const auto p1 = syscape::detail::process_list_backend::find_process<
        fake_context_reader>(1U);
    expect(p1.has_value(), "find_process(1) must find PID 1");
    if (p1) {
        expect(p1->pid == 1U && !p1->name.has_value(),
               "find_process(1) attributes must match");
    }

    const auto p_none = syscape::detail::process_list_backend::find_process<
        fake_context_reader>(999U);
    expect(!p_none && p_none.error() == syscape::errc::not_found,
           "find_process for nonexistent PID must report not_found");

    fake_context_reader::mock_data.clear();
    const auto procs_empty =
        syscape::detail::process_list_backend::processes<fake_context_reader>();
    expect(!procs_empty && procs_empty.error() == syscape::errc::not_supported,
           "missing context scheme must report not_supported");
}

void test_process_list_queries() {
    // On Linux host without /scheme/sys/context, live queries must report
    // not_supported.
    const auto count = syscape::process_list::process_count();
    expect(count.error() == syscape::errc::not_supported,
           "live process_count on host must report not_supported");

    const auto procs = syscape::process_list::processes();
    expect(procs.error() == syscape::errc::not_supported,
           "live processes on host must report not_supported");

    const auto p_none = syscape::process_list::find_process(99999999U);
    expect(p_none.error() == syscape::errc::not_supported,
           "live find_process on host must report not_supported");

    const auto matches = syscape::process_list::find_processes_by_name(
        "__nonexistent_proc_xyz_123__");
    expect(matches.error() == syscape::errc::not_supported,
           "name lookup must report not_supported on Redox OS");

    const auto empty_matches =
        syscape::process_list::find_processes_by_name("");
    expect(empty_matches.error() == syscape::errc::not_supported,
           "empty name lookup must report not_supported on Redox OS");
}

} // namespace

int main() {
    test_context_parsing();
    test_context_threads_deduplication();
    test_context_long_affinity();
    test_context_malformed_handling();
    test_context_wide_memory();
    test_context_pid0_and_stat_and_long_time();
    test_injected_process_list();
    test_process_list_queries();
    return failures == 0 ? 0 : 1;
}
