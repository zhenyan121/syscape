#ifndef SYSCAPE_DETAIL_IPC_COMMON_HPP
#define SYSCAPE_DETAIL_IPC_COMMON_HPP

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <syscape/detail/config.hpp>

namespace syscape {
namespace ipc {

/// IPC mechanism family for a resource.
enum class ipc_mechanism : std::uint8_t {
    /// Unknown or unclassified IPC mechanism.
    unknown = 0,
    /// POSIX IPC facility (/dev/shm, /dev/mqueue, named semaphores).
    posix = 1,
    /// System V IPC facility (/proc/sysvipc/*, shmget/msgget/semget).
    sysv = 2,
    /// Platform/system proprietary IPC facility.
    system = 3
};

/// Type classification for local/UNIX domain sockets.
enum class ipc_socket_type : std::uint8_t {
    /// Unknown or unspecified socket type.
    unknown = 0,
    /// Reliable sequenced stream socket (SOCK_STREAM).
    stream = 1,
    /// Connectionless datagram socket (SOCK_DGRAM).
    datagram = 2,
    /// Connection-oriented sequential packet socket (SOCK_SEQPACKET).
    sequential_packet = 3
};

/// Connection state for local/UNIX domain sockets.
enum class ipc_socket_state : std::uint8_t {
    /// Unknown or uninitialized socket state.
    unknown = 0,
    /// Unconnected / unbound socket.
    unconnected = 1,
    /// Socket is establishing a connection.
    connecting = 2,
    /// Socket connection is established.
    connected = 3,
    /// Socket is disconnecting or closing.
    disconnecting = 4,
    /// Socket is actively listening for incoming connections.
    listening = 5
};

/// Represents an allocated shared memory segment snapshot.
/// Values represent a point-in-time runtime snapshot and can change dynamically.
struct shared_memory_segment {
    /// The IPC mechanism family that created and manages this segment.
    ipc_mechanism mechanism = ipc_mechanism::unknown;

    /// System V `key_t` numeric key. `std::nullopt` for POSIX named shared memory.
    std::optional<std::int64_t> key;

    /// Numeric resource identifier (`shmid` for System V, inode number for POSIX).
    std::uint64_t id = 0;

    /// UTF-8 name or path identifying the segment (e.g. filename under /dev/shm for POSIX).
    std::string name;

    /// Size of the allocated shared memory segment in bytes.
    std::uint64_t size_bytes = 0;

    /// Octal permission mode bits (e.g. 0600 or 0666).
    std::uint32_t permissions = 0;

    /// Number of currently attached processes. `std::nullopt` for POSIX where not observable.
    std::optional<std::uint32_t> attached_processes;

    /// Numeric User ID of the segment creator.
    std::optional<std::uint32_t> creator_uid;

    /// Numeric Group ID of the segment creator.
    std::optional<std::uint32_t> creator_gid;

    /// Numeric User ID of the current segment owner.
    std::optional<std::uint32_t> owner_uid;

    /// Numeric Group ID of the current segment owner.
    std::optional<std::uint32_t> owner_gid;

    /// Process ID (PID) of the process that created the segment.
    std::optional<std::int64_t> creator_pid;

    /// Process ID (PID) of the last process that performed a control/attach change.
    std::optional<std::int64_t> last_change_pid;

    /// Timestamp of the last attach operation in seconds since Unix Epoch.
    /// `std::nullopt` for POSIX or when no attach has occurred.
    std::optional<std::uint64_t> last_attach_time;

    /// Timestamp of the last detach operation in seconds since Unix Epoch.
    /// `std::nullopt` for POSIX or when no detach has occurred.
    std::optional<std::uint64_t> last_detach_time;

    /// Timestamp of the last metadata change in seconds since Unix Epoch.
    /// `std::nullopt` when no change timestamp is recorded.
    std::optional<std::uint64_t> last_change_time;
};

/// Represents an active IPC message queue snapshot.
/// Values represent a point-in-time runtime snapshot and can change dynamically.
struct message_queue {
    /// The IPC mechanism family that created and manages this queue.
    ipc_mechanism mechanism = ipc_mechanism::unknown;

    /// System V `key_t` numeric key. `std::nullopt` for POSIX message queues.
    std::optional<std::int64_t> key;

    /// Numeric resource identifier (`msqid` for System V, inode number for POSIX).
    std::uint64_t id = 0;

    /// UTF-8 name identifying the queue (e.g. name in /dev/mqueue).
    std::string name;

    /// Number of messages currently buffered in the queue.
    /// `std::nullopt` for POSIX when queue attributes are not observable without opening.
    std::optional<std::uint64_t> current_messages;

    /// Maximum number of messages the queue can hold.
    /// `std::nullopt` when not observable or not applicable.
    std::optional<std::uint64_t> max_messages;

    /// Total bytes of message data currently stored in the queue.
    /// `std::nullopt` for POSIX when queue attributes are not observable without opening.
    std::optional<std::uint64_t> current_bytes;

    /// Maximum allowed message size in bytes for a single message in this queue.
    /// `std::nullopt` when not observable without opening.
    std::optional<std::uint64_t> max_message_bytes;

    /// Octal permission mode bits (e.g. 0660).
    std::uint32_t permissions = 0;

    /// Numeric User ID of the queue owner.
    std::optional<std::uint32_t> owner_uid;

    /// Numeric Group ID of the queue owner.
    std::optional<std::uint32_t> owner_gid;

    /// Process ID (PID) of the last process that sent a message to this queue.
    std::optional<std::int64_t> last_send_pid;

    /// Process ID (PID) of the last process that received a message from this queue.
    std::optional<std::int64_t> last_receive_pid;

    /// Timestamp of the last message sent in seconds since Unix Epoch.
    std::optional<std::uint64_t> last_send_time;

    /// Timestamp of the last message received in seconds since Unix Epoch.
    std::optional<std::uint64_t> last_receive_time;

    /// Timestamp of the last metadata change in seconds since Unix Epoch.
    std::optional<std::uint64_t> last_change_time;
};

/// Represents an active IPC semaphore or semaphore set snapshot.
/// Values represent a point-in-time runtime snapshot and can change dynamically.
struct semaphore_set {
    /// The IPC mechanism family that created and manages this semaphore set.
    ipc_mechanism mechanism = ipc_mechanism::unknown;

    /// System V `key_t` numeric key. `std::nullopt` for POSIX named semaphores.
    std::optional<std::int64_t> key;

    /// Numeric resource identifier (`semid` for System V, inode number for POSIX).
    std::uint64_t id = 0;

    /// UTF-8 name identifying the semaphore (e.g. semaphore name without "sem." prefix).
    std::string name;

    /// Number of semaphores contained within this set (1 for POSIX named semaphores).
    std::uint32_t semaphore_count = 0;

    /// Octal permission mode bits (e.g. 0600).
    std::uint32_t permissions = 0;

    /// Numeric User ID of the semaphore set owner.
    std::optional<std::uint32_t> owner_uid;

    /// Numeric Group ID of the semaphore set owner.
    std::optional<std::uint32_t> owner_gid;

    /// Timestamp of the last semaphore operation (`semop`) in seconds since Unix Epoch.
    std::optional<std::uint64_t> last_operation_time;

    /// Timestamp of the last metadata change in seconds since Unix Epoch.
    std::optional<std::uint64_t> last_change_time;
};

/// Represents an active local UNIX domain socket endpoint snapshot.
/// Values represent a point-in-time runtime snapshot within the current network/mount namespace.
struct local_socket {
    /// UTF-8 filesystem path or abstract name (starting with '@') of the socket endpoint.
    std::string path;

    /// VFS inode number identifying the socket endpoint.
    std::uint64_t inode = 0;

    /// Socket communication type (stream, datagram, or sequential packet).
    ipc_socket_type type = ipc_socket_type::unknown;

    /// Socket connection state (unconnected, connecting, connected, disconnecting, listening).
    ipc_socket_state state = ipc_socket_state::unknown;

    /// Internal kernel reference count for the socket.
    std::uint32_t ref_count = 0;

    /// Observable Process IDs (PIDs) holding open descriptors referencing this socket,
    /// subject to caller permissions and visibility in the current PID namespace.
    std::vector<std::int64_t> process_ids;
};

/// System-wide or namespace-wide IPC limits and kernel configuration parameters.
struct ipc_limits {
    /// Maximum size of a single shared memory segment in bytes (`shmmax`).
    std::optional<std::uint64_t> max_shared_memory_segment_bytes;

    /// Maximum total shared memory allocation across all segments in pages (`shmall`).
    std::optional<std::uint64_t> max_total_shared_memory_pages;

    /// Maximum number of shared memory segments allowed in the namespace (`shmmni`).
    std::optional<std::uint64_t> max_shared_memory_segments_system;

    /// Maximum allowed message size in bytes for a single message (`msgmax`).
    std::optional<std::uint64_t> max_message_bytes;

    /// Default maximum byte capacity of a newly created message queue (`msgmnb`).
    std::optional<std::uint64_t> default_message_queue_bytes;

    /// Maximum number of message queues allowed in the namespace (`msgmni`).
    std::optional<std::uint64_t> max_message_queues_system;

    /// Maximum number of semaphores allowed across all semaphore sets (`semmns`).
    std::optional<std::uint64_t> max_semaphores_system;

    /// Maximum number of semaphores allowed per individual semaphore set (`semmsl`).
    std::optional<std::uint32_t> max_semaphores_per_set;
};

} // namespace ipc

namespace detail {
namespace ipc_common {

inline bool compare_shared_memory_segments(
    const ::syscape::ipc::shared_memory_segment& lhs,
    const ::syscape::ipc::shared_memory_segment& rhs) noexcept {
    if (lhs.mechanism != rhs.mechanism) {
        return static_cast<std::uint8_t>(lhs.mechanism) <
               static_cast<std::uint8_t>(rhs.mechanism);
    }
    if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
    }
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    return lhs.name < rhs.name;
}

inline bool compare_message_queues(
    const ::syscape::ipc::message_queue& lhs,
    const ::syscape::ipc::message_queue& rhs) noexcept {
    if (lhs.mechanism != rhs.mechanism) {
        return static_cast<std::uint8_t>(lhs.mechanism) <
               static_cast<std::uint8_t>(rhs.mechanism);
    }
    if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
    }
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    return lhs.name < rhs.name;
}

inline bool compare_semaphore_sets(
    const ::syscape::ipc::semaphore_set& lhs,
    const ::syscape::ipc::semaphore_set& rhs) noexcept {
    if (lhs.mechanism != rhs.mechanism) {
        return static_cast<std::uint8_t>(lhs.mechanism) <
               static_cast<std::uint8_t>(rhs.mechanism);
    }
    if (lhs.key != rhs.key) {
        return lhs.key < rhs.key;
    }
    if (lhs.id != rhs.id) {
        return lhs.id < rhs.id;
    }
    return lhs.name < rhs.name;
}

inline bool compare_local_sockets(
    const ::syscape::ipc::local_socket& lhs,
    const ::syscape::ipc::local_socket& rhs) noexcept {
    if (lhs.path != rhs.path) {
        return lhs.path < rhs.path;
    }
    if (lhs.inode != rhs.inode) {
        return lhs.inode < rhs.inode;
    }
    if (lhs.type != rhs.type) {
        return static_cast<std::uint8_t>(lhs.type) <
               static_cast<std::uint8_t>(rhs.type);
    }
    return static_cast<std::uint8_t>(lhs.state) <
           static_cast<std::uint8_t>(rhs.state);
}

} // namespace ipc_common
} // namespace detail
} // namespace syscape

#endif // SYSCAPE_DETAIL_IPC_COMMON_HPP
