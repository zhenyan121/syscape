#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#include <syscape/detail/ipc/linux.hpp>
#include <syscape/ipc.hpp>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void test_parse_sysv_shm() {
    const char* sample =
        "       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime                   rss                  swap\n"
        " -123456788     131072   600               4194304  1234  5678      2  1000  1000  1000  1000 1724921000 1724921050 1724920000                     0                     0\n";

    const auto parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm(sample);
    expect(parsed.has_value(), "Valid sysv shm table with negative key must parse");
    if (parsed) {
        expect(parsed->size() == 1U, "Must parse 1 shm segment");
        const auto& seg = (*parsed)[0];
        expect(seg.mechanism == syscape::ipc::ipc_mechanism::sysv, "Mechanism must be sysv");
        expect(seg.key && *seg.key == -123456788LL, "Signed negative key must match");
        expect(seg.id == 131072ULL, "ID must be 131072");
        expect(seg.permissions == 0600U, "Permissions must be 0600 octal");
        expect(seg.size_bytes == 4194304ULL, "Size must be 4194304 bytes");
        expect(seg.creator_pid && *seg.creator_pid == 1234, "Creator PID must be 1234");
        expect(seg.last_change_pid && *seg.last_change_pid == 5678, "Last change PID must be 5678");
        expect(seg.attached_processes && *seg.attached_processes == 2U, "Attached processes must be 2");
        expect(seg.owner_uid && *seg.owner_uid == 1000U, "Owner UID must be 1000");
        expect(seg.owner_gid && *seg.owner_gid == 1000U, "Owner GID must be 1000");
        expect(seg.creator_uid && *seg.creator_uid == 1000U, "Creator UID must be 1000");
        expect(seg.creator_gid && *seg.creator_gid == 1000U, "Creator GID must be 1000");
        expect(seg.last_attach_time && *seg.last_attach_time == 1724921000ULL, "Last attach time match");
        expect(seg.last_detach_time && *seg.last_detach_time == 1724921050ULL, "Last detach time match");
        expect(seg.last_change_time && *seg.last_change_time == 1724920000ULL, "Last change time match");
    }

    const char* empty_sample =
        "       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime                   rss                  swap\n";
    const auto empty_parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm(empty_sample);
    expect(empty_parsed.has_value() && empty_parsed->empty(), "Empty table with valid header returns empty vector");

    // Rejection of empty/garbage headers
    const auto totally_empty = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm("");
    expect(!totally_empty && totally_empty.error() == syscape::errc::malformed_data,
           "Completely empty string fails as malformed_data");

    const auto newline_only = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm("\n");
    expect(!newline_only && newline_only.error() == syscape::errc::malformed_data,
           "Newline-only string fails as malformed_data");

    const auto garbage_header = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm("garbage header\n");
    expect(!garbage_header && garbage_header.error() == syscape::errc::malformed_data,
           "Garbage header fails as malformed_data");

    const auto reordered_shm_header = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm(
        "key shmid perms cpid size lpid nattch uid gid cuid cgid atime dtime ctime\n");
    expect(!reordered_shm_header && reordered_shm_header.error() == syscape::errc::malformed_data,
           "Reordered shm header columns fail as malformed_data");

    const char* truncated_sample =
        "       key      shmid perms                  size\n"
        "         0     131072   600\n";
    const auto trunc_parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm(truncated_sample);
    expect(!trunc_parsed && trunc_parsed.error() == syscape::errc::malformed_data,
           "Truncated line fails as malformed_data");

    const char* nonnum_sample =
        "       key      shmid perms                  size  cpid  lpid nattch   uid   gid  cuid  cgid      atime      dtime      ctime\n"
        "         0        abc   600               4194304     0     0      0     0     0     0     0          0          0          0\n";
    const auto nonnum_parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_shm(nonnum_sample);
    expect(!nonnum_parsed && nonnum_parsed.error() == syscape::errc::malformed_data,
           "Non-numeric ID fails as malformed_data");
}

void test_parse_sysv_msg() {
    const char* sample =
        "       key      msqid perms      cbytes       qnum lspid lrpid   uid   gid  cuid  cgid      stime      rtime      ctime\n"
        " -99999999          0   660          1024         10   111   222  1000  1000  1000  1000 1724920010 1724920020 1724920000\n";

    const auto parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_msg(sample);
    expect(parsed.has_value(), "Valid sysv msg table with negative key must parse");
    if (parsed) {
        expect(parsed->size() == 1U, "Must parse 1 msg queue");
        const auto& mq = (*parsed)[0];
        expect(mq.mechanism == syscape::ipc::ipc_mechanism::sysv, "Mechanism must be sysv");
        expect(mq.key && *mq.key == -99999999LL, "Signed negative key must match");
        expect(mq.id == 0U, "ID must be 0");
        expect(mq.permissions == 0660U, "Permissions must be 0660");
        expect(mq.current_bytes && *mq.current_bytes == 1024ULL, "Current bytes must be 1024");
        expect(mq.current_messages && *mq.current_messages == 10ULL, "Current messages must be 10");
        expect(mq.last_send_pid && *mq.last_send_pid == 111, "Last send PID must be 111");
        expect(mq.last_receive_pid && *mq.last_receive_pid == 222, "Last receive PID must be 222");
        expect(mq.last_send_time && *mq.last_send_time == 1724920010ULL, "Last send time match");
        expect(mq.last_receive_time && *mq.last_receive_time == 1724920020ULL, "Last receive time match");
        expect(mq.last_change_time && *mq.last_change_time == 1724920000ULL, "Last change time match");
    }

    const auto empty_msg = syscape::detail::ipc_backend::linux_impl::parse_sysv_msg("");
    expect(!empty_msg && empty_msg.error() == syscape::errc::malformed_data,
           "Empty msg table fails as malformed_data");

    const auto garbage_msg = syscape::detail::ipc_backend::linux_impl::parse_sysv_msg("random string\n");
    expect(!garbage_msg && garbage_msg.error() == syscape::errc::malformed_data,
           "Garbage msg header fails as malformed_data");

    const auto reordered_msg_header = syscape::detail::ipc_backend::linux_impl::parse_sysv_msg(
        "key msqid perms qnum cbytes lspid lrpid uid gid cuid cgid stime rtime ctime\n");
    expect(!reordered_msg_header && reordered_msg_header.error() == syscape::errc::malformed_data,
           "Reordered msg header columns fail as malformed_data");

    const char* invalid_sample =
        "       key      msqid perms\n"
        "       bad       bad   bad\n";
    const auto inv_parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_msg(invalid_sample);
    expect(!inv_parsed && inv_parsed.error() == syscape::errc::malformed_data,
           "Invalid line fails as malformed_data");
}

void test_parse_sysv_sem() {
    const char* sample =
        "       key      semid perms      nsems   uid   gid  cuid  cgid      otime      ctime\n"
        " -55555555          1   600          4  1000  1000  1000  1000 1724920050 1724920000\n";

    const auto parsed = syscape::detail::ipc_backend::linux_impl::parse_sysv_sem(sample);
    expect(parsed.has_value(), "Valid sysv sem table with negative key must parse");
    if (parsed) {
        expect(parsed->size() == 1U, "Must parse 1 sem set");
        const auto& sem = (*parsed)[0];
        expect(sem.mechanism == syscape::ipc::ipc_mechanism::sysv, "Mechanism must be sysv");
        expect(sem.key && *sem.key == -55555555LL, "Signed negative key must match");
        expect(sem.id == 1ULL, "ID must be 1");
        expect(sem.permissions == 0600U, "Permissions must be 0600");
        expect(sem.semaphore_count == 4U, "Semaphore count must be 4");
        expect(sem.last_operation_time && *sem.last_operation_time == 1724920050ULL, "Last op time match");
        expect(sem.last_change_time && *sem.last_change_time == 1724920000ULL, "Last change time match");
    }

    const auto empty_sem = syscape::detail::ipc_backend::linux_impl::parse_sysv_sem("");
    expect(!empty_sem && empty_sem.error() == syscape::errc::malformed_data,
           "Empty sem table fails as malformed_data");

    const auto garbage_sem = syscape::detail::ipc_backend::linux_impl::parse_sysv_sem("not a valid sem header\n");
    expect(!garbage_sem && garbage_sem.error() == syscape::errc::malformed_data,
           "Garbage sem header fails as malformed_data");

    const auto renamed_sem_header = syscape::detail::ipc_backend::linux_impl::parse_sysv_sem(
        "key semid perms count uid gid cuid cgid otime ctime\n");
    expect(!renamed_sem_header && renamed_sem_header.error() == syscape::errc::malformed_data,
           "Renamed sem header column fails as malformed_data");
}

void test_parse_proc_net_unix() {
    const char* sample =
        "Num       RefCount Protocol Flags    Type St Inode Path\n"
        "000000007cb88c14: 00000003 00000000 00000000 0001 03 29614\n"
        "00000000b6a7b24c: 00000002 00000000 00010000 0001 01 27163 /run/user/1000/bus\n"
        "00000000e386741a: 00000001 00000000 00000000 0002 01 13881 @/org/freedesktop/systemd1/notify\n"
        "00000000ffffffff: 00000001 00000000 00000000 0005 02 44444 /tmp/seqpacket.sock\n";

    syscape::detail::ipc_backend::linux_impl::inode_pid_map_type pid_map;
    pid_map[27163ULL] = {1001, 1002};

    const auto parsed = syscape::detail::ipc_backend::linux_impl::parse_proc_net_unix(sample, pid_map);
    expect(parsed.has_value(), "Valid /proc/net/unix table must parse");
    if (parsed) {
        expect(parsed->size() == 4U, "Must parse 4 sockets");

        // Unnamed stream socket
        const auto& s0 = (*parsed)[0];
        expect(s0.inode == 29614ULL, "s0 inode match");
        expect(s0.path.empty(), "s0 path empty");
        expect(s0.type == syscape::ipc::ipc_socket_type::stream, "s0 type stream");
        expect(s0.state == syscape::ipc::ipc_socket_state::connected, "s0 state connected");
        expect(s0.ref_count == 3U, "s0 ref count match");

        // Listening socket with path
        const auto& s1 = (*parsed)[1];
        expect(s1.inode == 27163ULL, "s1 inode match");
        expect(s1.path == "/run/user/1000/bus", "s1 path match");
        expect(s1.type == syscape::ipc::ipc_socket_type::stream, "s1 type stream");
        expect(s1.state == syscape::ipc::ipc_socket_state::listening, "s1 state listening");
        expect(s1.process_ids.size() == 2U && s1.process_ids[0] == 1001 && s1.process_ids[1] == 1002,
               "s1 process IDs matched injected map");

        // Abstract socket (starts with @)
        const auto& s2 = (*parsed)[2];
        expect(s2.inode == 13881ULL, "s2 inode match");
        expect(s2.path == "@/org/freedesktop/systemd1/notify", "s2 abstract path match");
        expect(s2.type == syscape::ipc::ipc_socket_type::datagram, "s2 type datagram");
        expect(s2.state == syscape::ipc::ipc_socket_state::unconnected, "s2 state unconnected");

        // Seqpacket socket
        const auto& s3 = (*parsed)[3];
        expect(s3.inode == 44444ULL, "s3 inode match");
        expect(s3.type == syscape::ipc::ipc_socket_type::sequential_packet, "s3 type seqpacket");
        expect(s3.state == syscape::ipc::ipc_socket_state::connecting, "s3 state connecting");
    }

    const auto garbage_unix = syscape::detail::ipc_backend::linux_impl::parse_proc_net_unix("bad header\n", pid_map);
    expect(!garbage_unix && garbage_unix.error() == syscape::errc::malformed_data,
           "Garbage unix header fails as malformed_data");

    const auto reordered_unix_header = syscape::detail::ipc_backend::linux_impl::parse_proc_net_unix(
        "Num RefCount Protocol Type Flags St Inode Path\n", pid_map);
    expect(!reordered_unix_header && reordered_unix_header.error() == syscape::errc::malformed_data,
           "Reordered unix header columns fail as malformed_data");

    const char* bad_flags_sample =
        "Num       RefCount Protocol Flags    Type St Inode Path\n"
        "000000007cb88c14: 00000003 00000000 zzzzzzzz 0001 03 29614\n";
    const auto bad_parsed = syscape::detail::ipc_backend::linux_impl::parse_proc_net_unix(bad_flags_sample, pid_map);
    expect(!bad_parsed && bad_parsed.error() == syscape::errc::malformed_data,
           "Non-hex flags must fail as malformed_data");

    // Invalid UTF-8 socket path test
    std::string invalid_utf8_sample =
        "Num       RefCount Protocol Flags    Type St Inode Path\n"
        "000000007cb88c14: 00000001 00000000 00000000 0001 01 10000 /tmp/bad_\xFF\xFE.sock\n";
    const auto utf8_err = syscape::detail::ipc_backend::linux_impl::parse_proc_net_unix(invalid_utf8_sample, pid_map);
    expect(!utf8_err && utf8_err.error() == syscape::errc::invalid_encoding,
           "Invalid UTF-8 in socket path must fail with invalid_encoding");
}

void test_missing_data_sources_fallback() {
    const auto shm_none = syscape::detail::ipc_backend::linux_impl::shared_memory_segments_from(
        "/nonexistent/proc/sysvipc/shm",
        "/nonexistent/dev/shm");
    expect(!shm_none && shm_none.error() == syscape::errc::not_supported,
           "Missing all SHM sources must return not_supported");

    const auto msg_none = syscape::detail::ipc_backend::linux_impl::message_queues_from(
        "/nonexistent/proc/sysvipc/msg",
        "/nonexistent/dev/mqueue");
    expect(!msg_none && msg_none.error() == syscape::errc::not_supported,
           "Missing all MSG sources must return not_supported");

    const auto sem_none = syscape::detail::ipc_backend::linux_impl::semaphore_sets_from(
        "/nonexistent/proc/sysvipc/sem",
        "/nonexistent/dev/shm");
    expect(!sem_none && sem_none.error() == syscape::errc::not_supported,
           "Missing all SEM sources must return not_supported");

    const auto lim_none = syscape::detail::ipc_backend::linux_impl::limits_from(
        "/nonexistent/proc/sys/kernel");
    expect(!lim_none && lim_none.error() == syscape::errc::not_supported,
           "Missing all limits files must return not_supported");
}

void test_live_queries() {
    const auto shm_res = syscape::ipc::shared_memory_segments();
    expect(shm_res.has_value() || shm_res.error() == syscape::errc::not_supported,
           "Live shared_memory_segments() must succeed or honestly report not_supported");

    const auto msg_res = syscape::ipc::message_queues();
    expect(msg_res.has_value() || msg_res.error() == syscape::errc::not_supported,
           "Live message_queues() must succeed or honestly report not_supported");

    const auto sem_res = syscape::ipc::semaphore_sets();
    expect(sem_res.has_value() || sem_res.error() == syscape::errc::not_supported,
           "Live semaphore_sets() must succeed or honestly report not_supported");

    const auto sock_res = syscape::ipc::local_sockets();
    expect(sock_res.has_value(), "Live local_sockets() must succeed on Linux host");
    if (sock_res) {
        // Verify deterministic sorting
        for (std::size_t i = 1; i < sock_res->size(); ++i) {
            const auto& prev = (*sock_res)[i - 1];
            const auto& curr = (*sock_res)[i];
            expect(!syscape::detail::ipc_common::compare_local_sockets(curr, prev),
                   "Local sockets must be deterministically sorted");
        }
    }

    const auto lim_res = syscape::ipc::limits();
    expect(lim_res.has_value(), "Live limits() must succeed on Linux host");
    if (lim_res) {
        expect(lim_res->max_shared_memory_segment_bytes.has_value(),
               "max_shared_memory_segment_bytes should be present on Linux");
        expect(lim_res->max_semaphores_system.has_value(),
               "max_semaphores_system should be present on Linux");
        expect(lim_res->max_message_bytes.has_value(),
               "max_message_bytes should be present on Linux");
        expect(lim_res->default_message_queue_bytes.has_value(),
               "default_message_queue_bytes should be present on Linux");
    }
}

} // namespace

int main() {
    test_parse_sysv_shm();
    test_parse_sysv_msg();
    test_parse_sysv_sem();
    test_parse_proc_net_unix();
    test_missing_data_sources_fallback();
    test_live_queries();

    if (failures != 0) {
        std::cerr << failures << " test(s) failed.\n";
        return 1;
    }
    std::cout << "All IPC tests passed successfully.\n";
    return 0;
}
