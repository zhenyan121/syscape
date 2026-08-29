#ifndef SYSCAPE_DETAIL_IPC_LINUX_HPP
#define SYSCAPE_DETAIL_IPC_LINUX_HPP

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <type_traits>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include <syscape/detail/ipc/common.hpp>
#include <syscape/detail/utf8.hpp>
#include <syscape/error.hpp>
#include <syscape/result.hpp>

namespace syscape {
namespace detail {
namespace ipc_backend {

namespace linux_impl {

template <typename UInt>
inline bool parse_unsigned(
    const std::string& text,
    int base,
    UInt& value) noexcept {
    static_assert(std::is_unsigned<UInt>::value, "UInt must be unsigned");
    if (text.empty()) {
        return false;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value, base);
    return parsed.ec == std::errc() && parsed.ptr == last;
}

template <typename Int>
inline bool parse_signed(
    const std::string& text,
    int base,
    Int& value) noexcept {
    static_assert(std::is_signed<Int>::value, "Int must be signed");
    if (text.empty()) {
        return false;
    }
    const char* const first = text.data();
    const char* const last = first + text.size();
    const std::from_chars_result parsed = std::from_chars(first, last, value, base);
    return parsed.ec == std::errc() && parsed.ptr == last;
}

inline std::vector<std::string> split_whitespace(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream iss(line);
    std::string tok;
    while (iss >> tok) {
        tokens.push_back(std::move(tok));
    }
    return tokens;
}

template <std::size_t N>
inline bool header_starts_with(
    const std::vector<std::string>& tokens,
    const char* const (&expected)[N]) noexcept {
    if (tokens.size() < N) {
        return false;
    }
    for (std::size_t index = 0; index < N; ++index) {
        if (tokens[index] != expected[index]) {
            return false;
        }
    }
    return true;
}

inline result<std::string> read_file_content(const std::string& path) {
    const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) {
        const int err = errno;
        if (err == ENOENT || err == ENOTDIR) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == EMFILE || err == ENFILE) {
            return fail(errc::resource_exhausted);
        }
        return fail(std::error_code(err, std::system_category()));
    }

    std::string buffer;
    char chunk[4096];
    while (true) {
        const ssize_t count = ::read(fd, chunk, sizeof(chunk));
        if (count < 0) {
            const int err = errno;
            if (err == EINTR) {
                continue;
            }
            ::close(fd);
            return fail(std::error_code(err, std::system_category()));
        }
        if (count == 0) {
            break;
        }
        buffer.append(chunk, static_cast<std::size_t>(count));
    }
    ::close(fd);
    return buffer;
}

/// Parses the contents of /proc/sysvipc/shm.
inline result<std::vector<::syscape::ipc::shared_memory_segment>>
parse_sysv_shm(const std::string& content) {
    std::vector<::syscape::ipc::shared_memory_segment> list;
    if (content.empty()) {
        return fail(errc::malformed_data);
    }
    std::istringstream stream(content);
    std::string line;

    // Header line
    if (!std::getline(stream, line)) {
        return fail(errc::malformed_data);
    }

    static const char* const expected_header[] = {
        "key", "shmid", "perms", "size", "cpid", "lpid", "nattch",
        "uid", "gid", "cuid", "cgid", "atime", "dtime", "ctime"};
    const auto header_tokens = split_whitespace(line);
    if (!header_starts_with(header_tokens, expected_header)) {
        return fail(errc::malformed_data);
    }

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> cols = split_whitespace(line);
        if (cols.size() < 14) {
            return fail(errc::malformed_data);
        }

        ::syscape::ipc::shared_memory_segment seg;
        seg.mechanism = ::syscape::ipc::ipc_mechanism::sysv;

        std::int64_t k = 0;
        if (!parse_signed(cols[0], 10, k)) {
            return fail(errc::malformed_data);
        }
        seg.key = k;

        if (!parse_unsigned(cols[1], 10, seg.id)) {
            return fail(errc::malformed_data);
        }
        if (!parse_unsigned(cols[2], 8, seg.permissions)) {
            return fail(errc::malformed_data);
        }
        if (!parse_unsigned(cols[3], 10, seg.size_bytes)) {
            return fail(errc::malformed_data);
        }

        std::int64_t cpid = 0;
        if (!parse_signed(cols[4], 10, cpid)) {
            return fail(errc::malformed_data);
        }
        if (cpid > 0) {
            seg.creator_pid = cpid;
        }

        std::int64_t lpid = 0;
        if (!parse_signed(cols[5], 10, lpid)) {
            return fail(errc::malformed_data);
        }
        if (lpid > 0) {
            seg.last_change_pid = lpid;
        }

        std::uint32_t nattch = 0;
        if (!parse_unsigned(cols[6], 10, nattch)) {
            return fail(errc::malformed_data);
        }
        seg.attached_processes = nattch;

        std::uint32_t uid = 0;
        if (!parse_unsigned(cols[7], 10, uid)) {
            return fail(errc::malformed_data);
        }
        seg.owner_uid = uid;

        std::uint32_t gid = 0;
        if (!parse_unsigned(cols[8], 10, gid)) {
            return fail(errc::malformed_data);
        }
        seg.owner_gid = gid;

        std::uint32_t cuid = 0;
        if (!parse_unsigned(cols[9], 10, cuid)) {
            return fail(errc::malformed_data);
        }
        seg.creator_uid = cuid;

        std::uint32_t cgid = 0;
        if (!parse_unsigned(cols[10], 10, cgid)) {
            return fail(errc::malformed_data);
        }
        seg.creator_gid = cgid;

        std::uint64_t atime = 0;
        if (!parse_unsigned(cols[11], 10, atime)) {
            return fail(errc::malformed_data);
        }
        if (atime > 0) {
            seg.last_attach_time = atime;
        }

        std::uint64_t dtime = 0;
        if (!parse_unsigned(cols[12], 10, dtime)) {
            return fail(errc::malformed_data);
        }
        if (dtime > 0) {
            seg.last_detach_time = dtime;
        }

        std::uint64_t ctime = 0;
        if (!parse_unsigned(cols[13], 10, ctime)) {
            return fail(errc::malformed_data);
        }
        if (ctime > 0) {
            seg.last_change_time = ctime;
        }

        list.push_back(std::move(seg));
    }

    return list;
}

/// Parses POSIX shared memory objects located in /dev/shm.
inline result<void> scan_posix_shm(
    const std::string& dir_path,
    std::vector<::syscape::ipc::shared_memory_segment>& list) {
    DIR* const dir = ::opendir(dir_path.c_str());
    if (dir == nullptr) {
        const int err = errno;
        if (err == ENOENT || err == ENOTDIR) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == EMFILE || err == ENFILE) {
            return fail(errc::resource_exhausted);
        }
        return fail(std::error_code(err, std::system_category()));
    }

    struct dir_closer {
        DIR* d;
        ~dir_closer() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } closer{dir};

    while (true) {
        errno = 0;
        struct dirent* const entry = ::readdir(dir);
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::system_category()));
            }
            break;
        }

        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        // Named semaphores in /dev/shm start with "sem."
        if (name.rfind("sem.", 0) == 0) {
            continue;
        }
        if (!detail::is_valid_utf8(name)) {
            return fail(errc::invalid_encoding);
        }

        const std::string full_path = dir_path + "/" + name;
        struct stat st{};
        if (::lstat(full_path.c_str(), &st) != 0) {
            const int err = errno;
            if (err == ENOENT) {
                continue;
            }
            return fail(std::error_code(err, std::system_category()));
        }

        // Only regular files qualify as candidate POSIX shared memory objects
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        ::syscape::ipc::shared_memory_segment seg;
        seg.mechanism = ::syscape::ipc::ipc_mechanism::posix;
        seg.key = std::nullopt;
        seg.id = static_cast<std::uint64_t>(st.st_ino);
        seg.name = name;
        seg.size_bytes = static_cast<std::uint64_t>(st.st_size > 0 ? st.st_size : 0);
        seg.permissions = static_cast<std::uint32_t>(st.st_mode & 07777U);
        seg.attached_processes = std::nullopt;
        seg.owner_uid = static_cast<std::uint32_t>(st.st_uid);
        seg.owner_gid = static_cast<std::uint32_t>(st.st_gid);
        // Attach and detach timestamps do not exist in POSIX shm

        list.push_back(std::move(seg));
    }

    return {};
}

/// Parses the contents of /proc/sysvipc/msg.
inline result<std::vector<::syscape::ipc::message_queue>>
parse_sysv_msg(const std::string& content) {
    std::vector<::syscape::ipc::message_queue> list;
    if (content.empty()) {
        return fail(errc::malformed_data);
    }
    std::istringstream stream(content);
    std::string line;

    if (!std::getline(stream, line)) {
        return fail(errc::malformed_data);
    }

    static const char* const expected_header[] = {
        "key", "msqid", "perms", "cbytes", "qnum", "lspid", "lrpid",
        "uid", "gid", "cuid", "cgid", "stime", "rtime", "ctime"};
    const auto header_tokens = split_whitespace(line);
    if (!header_starts_with(header_tokens, expected_header)) {
        return fail(errc::malformed_data);
    }

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> cols = split_whitespace(line);
        if (cols.size() < 14) {
            return fail(errc::malformed_data);
        }

        ::syscape::ipc::message_queue mq;
        mq.mechanism = ::syscape::ipc::ipc_mechanism::sysv;

        std::int64_t k = 0;
        if (!parse_signed(cols[0], 10, k)) {
            return fail(errc::malformed_data);
        }
        mq.key = k;

        if (!parse_unsigned(cols[1], 10, mq.id)) {
            return fail(errc::malformed_data);
        }
        if (!parse_unsigned(cols[2], 8, mq.permissions)) {
            return fail(errc::malformed_data);
        }

        std::uint64_t cbytes = 0;
        if (!parse_unsigned(cols[3], 10, cbytes)) {
            return fail(errc::malformed_data);
        }
        mq.current_bytes = cbytes;

        std::uint64_t qnum = 0;
        if (!parse_unsigned(cols[4], 10, qnum)) {
            return fail(errc::malformed_data);
        }
        mq.current_messages = qnum;

        std::int64_t lspid = 0;
        if (!parse_signed(cols[5], 10, lspid)) {
            return fail(errc::malformed_data);
        }
        if (lspid > 0) {
            mq.last_send_pid = lspid;
        }

        std::int64_t lrpid = 0;
        if (!parse_signed(cols[6], 10, lrpid)) {
            return fail(errc::malformed_data);
        }
        if (lrpid > 0) {
            mq.last_receive_pid = lrpid;
        }

        std::uint32_t uid = 0;
        if (!parse_unsigned(cols[7], 10, uid)) {
            return fail(errc::malformed_data);
        }
        mq.owner_uid = uid;

        std::uint32_t gid = 0;
        if (!parse_unsigned(cols[8], 10, gid)) {
            return fail(errc::malformed_data);
        }
        mq.owner_gid = gid;

        std::uint32_t cuid = 0;
        if (!parse_unsigned(cols[9], 10, cuid)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t cgid = 0;
        if (!parse_unsigned(cols[10], 10, cgid)) {
            return fail(errc::malformed_data);
        }

        std::uint64_t stime = 0;
        if (!parse_unsigned(cols[11], 10, stime)) {
            return fail(errc::malformed_data);
        }
        if (stime > 0) {
            mq.last_send_time = stime;
        }

        std::uint64_t rtime = 0;
        if (!parse_unsigned(cols[12], 10, rtime)) {
            return fail(errc::malformed_data);
        }
        if (rtime > 0) {
            mq.last_receive_time = rtime;
        }

        std::uint64_t ctime = 0;
        if (!parse_unsigned(cols[13], 10, ctime)) {
            return fail(errc::malformed_data);
        }
        if (ctime > 0) {
            mq.last_change_time = ctime;
        }

        list.push_back(std::move(mq));
    }

    return list;
}

/// Parses POSIX message queue objects located in /dev/mqueue.
inline result<void> scan_posix_mqueue(
    const std::string& dir_path,
    std::vector<::syscape::ipc::message_queue>& list) {
    DIR* const dir = ::opendir(dir_path.c_str());
    if (dir == nullptr) {
        const int err = errno;
        if (err == ENOENT || err == ENOTDIR) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == EMFILE || err == ENFILE) {
            return fail(errc::resource_exhausted);
        }
        return fail(std::error_code(err, std::system_category()));
    }

    struct dir_closer {
        DIR* d;
        ~dir_closer() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } closer{dir};

    while (true) {
        errno = 0;
        struct dirent* const entry = ::readdir(dir);
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::system_category()));
            }
            break;
        }

        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        if (!detail::is_valid_utf8(name)) {
            return fail(errc::invalid_encoding);
        }

        const std::string full_path = dir_path + "/" + name;
        struct stat st{};
        if (::lstat(full_path.c_str(), &st) != 0) {
            const int err = errno;
            if (err == ENOENT) {
                continue;
            }
            return fail(std::error_code(err, std::system_category()));
        }

        ::syscape::ipc::message_queue mq;
        mq.mechanism = ::syscape::ipc::ipc_mechanism::posix;
        mq.key = std::nullopt;
        mq.id = static_cast<std::uint64_t>(st.st_ino);
        mq.name = name;
        mq.permissions = static_cast<std::uint32_t>(st.st_mode & 07777U);
        mq.owner_uid = static_cast<std::uint32_t>(st.st_uid);
        mq.owner_gid = static_cast<std::uint32_t>(st.st_gid);

        list.push_back(std::move(mq));
    }

    return {};
}

/// Parses the contents of /proc/sysvipc/sem.
inline result<std::vector<::syscape::ipc::semaphore_set>>
parse_sysv_sem(const std::string& content) {
    std::vector<::syscape::ipc::semaphore_set> list;
    if (content.empty()) {
        return fail(errc::malformed_data);
    }
    std::istringstream stream(content);
    std::string line;

    if (!std::getline(stream, line)) {
        return fail(errc::malformed_data);
    }

    static const char* const expected_header[] = {
        "key", "semid", "perms", "nsems", "uid", "gid", "cuid", "cgid",
        "otime", "ctime"};
    const auto header_tokens = split_whitespace(line);
    if (!header_starts_with(header_tokens, expected_header)) {
        return fail(errc::malformed_data);
    }

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }
        const std::vector<std::string> cols = split_whitespace(line);
        if (cols.size() < 10) {
            return fail(errc::malformed_data);
        }

        ::syscape::ipc::semaphore_set sem;
        sem.mechanism = ::syscape::ipc::ipc_mechanism::sysv;

        std::int64_t k = 0;
        if (!parse_signed(cols[0], 10, k)) {
            return fail(errc::malformed_data);
        }
        sem.key = k;

        if (!parse_unsigned(cols[1], 10, sem.id)) {
            return fail(errc::malformed_data);
        }
        if (!parse_unsigned(cols[2], 8, sem.permissions)) {
            return fail(errc::malformed_data);
        }
        if (!parse_unsigned(cols[3], 10, sem.semaphore_count)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t uid = 0;
        if (!parse_unsigned(cols[4], 10, uid)) {
            return fail(errc::malformed_data);
        }
        sem.owner_uid = uid;

        std::uint32_t gid = 0;
        if (!parse_unsigned(cols[5], 10, gid)) {
            return fail(errc::malformed_data);
        }
        sem.owner_gid = gid;

        std::uint32_t cuid = 0;
        if (!parse_unsigned(cols[6], 10, cuid)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t cgid = 0;
        if (!parse_unsigned(cols[7], 10, cgid)) {
            return fail(errc::malformed_data);
        }

        std::uint64_t otime = 0;
        if (!parse_unsigned(cols[8], 10, otime)) {
            return fail(errc::malformed_data);
        }
        if (otime > 0) {
            sem.last_operation_time = otime;
        }

        std::uint64_t ctime = 0;
        if (!parse_unsigned(cols[9], 10, ctime)) {
            return fail(errc::malformed_data);
        }
        if (ctime > 0) {
            sem.last_change_time = ctime;
        }

        list.push_back(std::move(sem));
    }

    return list;
}

/// Parses POSIX named semaphores located in /dev/shm/sem.*.
inline result<void> scan_posix_semaphores(
    const std::string& dir_path,
    std::vector<::syscape::ipc::semaphore_set>& list) {
    DIR* const dir = ::opendir(dir_path.c_str());
    if (dir == nullptr) {
        const int err = errno;
        if (err == ENOENT || err == ENOTDIR) {
            return fail(errc::not_supported);
        }
        if (err == EACCES || err == EPERM) {
            return fail(errc::permission_denied);
        }
        if (err == EMFILE || err == ENFILE) {
            return fail(errc::resource_exhausted);
        }
        return fail(std::error_code(err, std::system_category()));
    }

    struct dir_closer {
        DIR* d;
        ~dir_closer() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } closer{dir};

    while (true) {
        errno = 0;
        struct dirent* const entry = ::readdir(dir);
        if (entry == nullptr) {
            if (errno != 0) {
                return fail(std::error_code(errno, std::system_category()));
            }
            break;
        }

        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        if (name.rfind("sem.", 0) != 0) {
            continue;
        }
        const std::string sem_name = name.substr(4);
        if (!detail::is_valid_utf8(sem_name)) {
            return fail(errc::invalid_encoding);
        }

        const std::string full_path = dir_path + "/" + name;
        struct stat st{};
        if (::lstat(full_path.c_str(), &st) != 0) {
            const int err = errno;
            if (err == ENOENT) {
                continue;
            }
            return fail(std::error_code(err, std::system_category()));
        }
        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        ::syscape::ipc::semaphore_set sem;
        sem.mechanism = ::syscape::ipc::ipc_mechanism::posix;
        sem.key = std::nullopt;
        sem.id = static_cast<std::uint64_t>(st.st_ino);
        sem.name = sem_name;
        sem.semaphore_count = 1U;
        sem.permissions = static_cast<std::uint32_t>(st.st_mode & 07777U);
        sem.owner_uid = static_cast<std::uint32_t>(st.st_uid);
        sem.owner_gid = static_cast<std::uint32_t>(st.st_gid);

        list.push_back(std::move(sem));
    }

    return {};
}

/// Inode to owning PID mapping built from scanning /proc/[pid]/fd.
using inode_pid_map_type = std::unordered_map<std::uint64_t, std::vector<std::int64_t>>;

inline inode_pid_map_type build_socket_inode_pid_map(const std::string& proc_dir = "/proc") {
    inode_pid_map_type map;
    DIR* const proc = ::opendir(proc_dir.c_str());
    if (proc == nullptr) {
        return map;
    }

    struct dir_closer {
        DIR* d;
        ~dir_closer() {
            if (d != nullptr) {
                ::closedir(d);
            }
        }
    } closer{proc};

    struct dirent* proc_entry = nullptr;
    while ((proc_entry = ::readdir(proc)) != nullptr) {
        if (proc_entry->d_name[0] < '0' || proc_entry->d_name[0] > '9') {
            continue;
        }

        std::int64_t pid = 0;
        if (!parse_signed(proc_entry->d_name, 10, pid) || pid <= 0) {
            continue;
        }

        const std::string fd_dir_path = proc_dir + "/" + proc_entry->d_name + "/fd";
        DIR* const fd_dir = ::opendir(fd_dir_path.c_str());
        if (fd_dir == nullptr) {
            continue;
        }

        struct dir_closer fd_closer{fd_dir};
        struct dirent* fd_entry = nullptr;
        char link_buf[256];
        while ((fd_entry = ::readdir(fd_dir)) != nullptr) {
            if (fd_entry->d_name[0] == '.') {
                continue;
            }

            const std::string fd_link = fd_dir_path + "/" + fd_entry->d_name;
            const ssize_t len = ::readlink(fd_link.c_str(), link_buf, sizeof(link_buf) - 1U);
            if (len <= 0) {
                continue;
            }
            link_buf[len] = '\0';

            // Target format: "socket:[12345]"
            if (std::strncmp(link_buf, "socket:[", 8) == 0 && len > 9 && link_buf[len - 1] == ']') {
                link_buf[len - 1] = '\0';
                const std::string inode_str(link_buf + 8);
                std::uint64_t inode = 0;
                if (parse_unsigned(inode_str, 10, inode)) {
                    std::vector<std::int64_t>& pids = map[inode];
                    if (std::find(pids.begin(), pids.end(), pid) == pids.end()) {
                        pids.push_back(pid);
                    }
                }
            }
        }
    }

    for (auto& pair : map) {
        std::sort(pair.second.begin(), pair.second.end());
    }

    return map;
}

/// Parses the contents of /proc/net/unix.
inline result<std::vector<::syscape::ipc::local_socket>>
parse_proc_net_unix(
    const std::string& content,
    const inode_pid_map_type& pid_map) {
    std::vector<::syscape::ipc::local_socket> list;
    if (content.empty()) {
        return fail(errc::malformed_data);
    }
    std::istringstream stream(content);
    std::string line;

    // Header line: "Num RefCount Protocol Flags Type St Inode Path"
    if (!std::getline(stream, line)) {
        return fail(errc::malformed_data);
    }

    static const char* const expected_header[] = {
        "Num", "RefCount", "Protocol", "Flags", "Type", "St", "Inode",
        "Path"};
    const auto header_tokens = split_whitespace(line);
    if (!header_starts_with(header_tokens, expected_header)) {
        return fail(errc::malformed_data);
    }

    while (std::getline(stream, line)) {
        if (line.empty()) {
            continue;
        }

        std::istringstream line_stream(line);
        std::string num_token;
        std::string ref_cnt_token;
        std::string proto_token;
        std::string flags_token;
        std::string type_token;
        std::string state_token;
        std::string inode_token;

        if (!(line_stream >> num_token >> ref_cnt_token >> proto_token >>
              flags_token >> type_token >> state_token >> inode_token)) {
            return fail(errc::malformed_data);
        }

        // Validate num token (hex address)
        if (!num_token.empty() && num_token.back() == ':') {
            num_token.pop_back();
        }
        std::uint64_t num_val = 0;
        if (!parse_unsigned(num_token, 16, num_val)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t proto_val = 0;
        if (!parse_unsigned(proto_token, 16, proto_val)) {
            return fail(errc::malformed_data);
        }

        std::string path;
        std::string remaining;
        std::getline(line_stream, remaining);
        const std::size_t first_non_space = remaining.find_first_not_of(" \t");
        if (first_non_space != std::string::npos) {
            path = remaining.substr(first_non_space);
        }
        if (!detail::is_valid_utf8(path)) {
            return fail(errc::invalid_encoding);
        }

        ::syscape::ipc::local_socket sock;
        sock.path = path;

        if (!parse_unsigned(inode_token, 10, sock.inode)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t ref_count = 0;
        if (!parse_unsigned(ref_cnt_token, 16, ref_count)) {
            return fail(errc::malformed_data);
        }
        sock.ref_count = ref_count;

        std::uint32_t flags = 0;
        if (!parse_unsigned(flags_token, 16, flags)) {
            return fail(errc::malformed_data);
        }

        std::uint32_t type_val = 0;
        if (!parse_unsigned(type_token, 16, type_val)) {
            return fail(errc::malformed_data);
        }
        switch (type_val) {
        case 1: sock.type = ::syscape::ipc::ipc_socket_type::stream; break;
        case 2: sock.type = ::syscape::ipc::ipc_socket_type::datagram; break;
        case 5: sock.type = ::syscape::ipc::ipc_socket_type::sequential_packet; break;
        default: sock.type = ::syscape::ipc::ipc_socket_type::unknown; break;
        }

        std::uint32_t state_val = 0;
        if (!parse_unsigned(state_token, 16, state_val)) {
            return fail(errc::malformed_data);
        }

        if ((flags & 0x00010000U) != 0U || (flags & 0x00020000U) != 0U) {
            sock.state = ::syscape::ipc::ipc_socket_state::listening;
        } else {
            switch (state_val) {
            case 1: sock.state = ::syscape::ipc::ipc_socket_state::unconnected; break;
            case 2: sock.state = ::syscape::ipc::ipc_socket_state::connecting; break;
            case 3: sock.state = ::syscape::ipc::ipc_socket_state::connected; break;
            case 4: sock.state = ::syscape::ipc::ipc_socket_state::disconnecting; break;
            default: sock.state = ::syscape::ipc::ipc_socket_state::unknown; break;
            }
        }

        const auto it = pid_map.find(sock.inode);
        if (it != pid_map.end()) {
            sock.process_ids = it->second;
        }

        list.push_back(std::move(sock));
    }

    return list;
}

inline result<std::vector<::syscape::ipc::shared_memory_segment>>
shared_memory_segments_from(const std::string& sysv_file, const std::string& posix_dir) {
    std::vector<::syscape::ipc::shared_memory_segment> result_list;
    bool any_source_supported = false;

    const auto sysv_content = read_file_content(sysv_file);
    if (sysv_content.has_value()) {
        auto parsed = parse_sysv_shm(*sysv_content);
        if (!parsed.has_value()) {
            return fail(parsed.error());
        }
        any_source_supported = true;
        for (auto& item : parsed.value()) {
            result_list.push_back(std::move(item));
        }
    } else if (sysv_content.error() != make_error_code(errc::not_supported)) {
        return fail(sysv_content.error());
    }

    const auto posix_res = scan_posix_shm(posix_dir, result_list);
    if (posix_res.has_value()) {
        any_source_supported = true;
    } else if (posix_res.error() != make_error_code(errc::not_supported)) {
        return fail(posix_res.error());
    }

    if (!any_source_supported) {
        return fail(errc::not_supported);
    }

    std::sort(
        result_list.begin(),
        result_list.end(),
        ipc_common::compare_shared_memory_segments);

    return result_list;
}

inline result<std::vector<::syscape::ipc::message_queue>>
message_queues_from(const std::string& sysv_file, const std::string& posix_dir) {
    std::vector<::syscape::ipc::message_queue> result_list;
    bool any_source_supported = false;

    const auto sysv_content = read_file_content(sysv_file);
    if (sysv_content.has_value()) {
        auto parsed = parse_sysv_msg(*sysv_content);
        if (!parsed.has_value()) {
            return fail(parsed.error());
        }
        any_source_supported = true;
        for (auto& item : parsed.value()) {
            result_list.push_back(std::move(item));
        }
    } else if (sysv_content.error() != make_error_code(errc::not_supported)) {
        return fail(sysv_content.error());
    }

    const auto posix_res = scan_posix_mqueue(posix_dir, result_list);
    if (posix_res.has_value()) {
        any_source_supported = true;
    } else if (posix_res.error() != make_error_code(errc::not_supported)) {
        return fail(posix_res.error());
    }

    if (!any_source_supported) {
        return fail(errc::not_supported);
    }

    std::sort(
        result_list.begin(),
        result_list.end(),
        ipc_common::compare_message_queues);

    return result_list;
}

inline result<std::vector<::syscape::ipc::semaphore_set>>
semaphore_sets_from(const std::string& sysv_file, const std::string& posix_dir) {
    std::vector<::syscape::ipc::semaphore_set> result_list;
    bool any_source_supported = false;

    const auto sysv_content = read_file_content(sysv_file);
    if (sysv_content.has_value()) {
        auto parsed = parse_sysv_sem(*sysv_content);
        if (!parsed.has_value()) {
            return fail(parsed.error());
        }
        any_source_supported = true;
        for (auto& item : parsed.value()) {
            result_list.push_back(std::move(item));
        }
    } else if (sysv_content.error() != make_error_code(errc::not_supported)) {
        return fail(sysv_content.error());
    }

    const auto posix_res = scan_posix_semaphores(posix_dir, result_list);
    if (posix_res.has_value()) {
        any_source_supported = true;
    } else if (posix_res.error() != make_error_code(errc::not_supported)) {
        return fail(posix_res.error());
    }

    if (!any_source_supported) {
        return fail(errc::not_supported);
    }

    std::sort(
        result_list.begin(),
        result_list.end(),
        ipc_common::compare_semaphore_sets);

    return result_list;
}

inline result<::syscape::ipc::ipc_limits>
limits_from(const std::string& sysctl_dir) {
    ::syscape::ipc::ipc_limits lim;
    bool any_limit_found = false;

    const auto shmmax = read_file_content(sysctl_dir + "/shmmax");
    if (shmmax.has_value()) {
        const auto tokens = split_whitespace(*shmmax);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.max_shared_memory_segment_bytes = val;
        any_limit_found = true;
    } else if (shmmax.error() != make_error_code(errc::not_supported)) {
        return fail(shmmax.error());
    }

    const auto shmall = read_file_content(sysctl_dir + "/shmall");
    if (shmall.has_value()) {
        const auto tokens = split_whitespace(*shmall);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.max_total_shared_memory_pages = val;
        any_limit_found = true;
    } else if (shmall.error() != make_error_code(errc::not_supported)) {
        return fail(shmall.error());
    }

    const auto shmmni = read_file_content(sysctl_dir + "/shmmni");
    if (shmmni.has_value()) {
        const auto tokens = split_whitespace(*shmmni);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.max_shared_memory_segments_system = val;
        any_limit_found = true;
    } else if (shmmni.error() != make_error_code(errc::not_supported)) {
        return fail(shmmni.error());
    }

    const auto msgmax = read_file_content(sysctl_dir + "/msgmax");
    if (msgmax.has_value()) {
        const auto tokens = split_whitespace(*msgmax);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.max_message_bytes = val;
        any_limit_found = true;
    } else if (msgmax.error() != make_error_code(errc::not_supported)) {
        return fail(msgmax.error());
    }

    const auto msgmnb = read_file_content(sysctl_dir + "/msgmnb");
    if (msgmnb.has_value()) {
        const auto tokens = split_whitespace(*msgmnb);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.default_message_queue_bytes = val;
        any_limit_found = true;
    } else if (msgmnb.error() != make_error_code(errc::not_supported)) {
        return fail(msgmnb.error());
    }

    const auto msgmni = read_file_content(sysctl_dir + "/msgmni");
    if (msgmni.has_value()) {
        const auto tokens = split_whitespace(*msgmni);
        if (tokens.empty()) {
            return fail(errc::malformed_data);
        }
        std::uint64_t val = 0;
        if (!parse_unsigned(tokens[0], 10, val)) {
            return fail(errc::malformed_data);
        }
        lim.max_message_queues_system = val;
        any_limit_found = true;
    } else if (msgmni.error() != make_error_code(errc::not_supported)) {
        return fail(msgmni.error());
    }

    const auto sem = read_file_content(sysctl_dir + "/sem");
    if (sem.has_value()) {
        const auto tokens = split_whitespace(*sem);
        if (tokens.size() < 2) {
            return fail(errc::malformed_data);
        }
        std::uint32_t semmsl = 0;
        if (!parse_unsigned(tokens[0], 10, semmsl)) {
            return fail(errc::malformed_data);
        }
        lim.max_semaphores_per_set = semmsl;

        std::uint64_t semmns = 0;
        if (!parse_unsigned(tokens[1], 10, semmns)) {
            return fail(errc::malformed_data);
        }
        lim.max_semaphores_system = semmns;
        any_limit_found = true;
    } else if (sem.error() != make_error_code(errc::not_supported)) {
        return fail(sem.error());
    }

    if (!any_limit_found) {
        return fail(errc::not_supported);
    }

    return lim;
}

} // namespace linux_impl

inline result<std::vector<::syscape::ipc::shared_memory_segment>>
shared_memory_segments() {
    return linux_impl::shared_memory_segments_from(
        "/proc/sysvipc/shm",
        "/dev/shm");
}

inline result<std::vector<::syscape::ipc::message_queue>> message_queues() {
    return linux_impl::message_queues_from(
        "/proc/sysvipc/msg",
        "/dev/mqueue");
}

inline result<std::vector<::syscape::ipc::semaphore_set>> semaphore_sets() {
    return linux_impl::semaphore_sets_from(
        "/proc/sysvipc/sem",
        "/dev/shm");
}

inline result<std::vector<::syscape::ipc::local_socket>> local_sockets() {
    const auto content = linux_impl::read_file_content("/proc/net/unix");
    if (!content.has_value()) {
        return fail(content.error());
    }

    const linux_impl::inode_pid_map_type pid_map =
        linux_impl::build_socket_inode_pid_map("/proc");

    auto parsed = linux_impl::parse_proc_net_unix(*content, pid_map);
    if (!parsed.has_value()) {
        return fail(parsed.error());
    }

    std::sort(
        parsed.value().begin(),
        parsed.value().end(),
        ipc_common::compare_local_sockets);

    return parsed;
}

inline result<::syscape::ipc::ipc_limits> limits() {
    return linux_impl::limits_from("/proc/sys/kernel");
}

} // namespace ipc_backend
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_IPC_LINUX_HPP
