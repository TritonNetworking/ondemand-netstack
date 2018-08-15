// Message to be exchanged between application and transport daemon

#ifndef DCCS_MESSAGE_H
#define DCCS_MESSAGE_H

#include <stdint.h>

/* @section: enum definitions */

enum class IPCOperation : uint8_t {
    NOP,
    GETADDRINFO,
    CONNECT,
    SOCKET,
    CLOSE
};

enum class MPIOperation : uint8_t {
    NOP,
    CONNECT,
};

enum class MPIStatus : uint8_t {
    SUCCESS,
    PERM,
    CONNREFUSED,
};


/* @section: control messages */

// Message sent across IPC (on the same machine), to wrap socket syscalls.

struct IPCRequest {
    int fd;
    IPCOperation operation;
    size_t length;
};

struct IPCResponse {
    union {
        int retval_int;
        ssize_t retval_ssize_t;
    };
    int error;
    size_t length;
};

// MPI message tags
#define MPI_TAG_REQUEST 1
#define MPI_TAG_RESPONSE 2

// Message sent across the MPI network.

struct MPIRequest {
    uint16_t src_port;
    uint16_t dst_port;
    enum MPIOperation operation;
};

struct MPIResponse {
    uint16_t src_port;
    uint16_t dst_port;
    enum MPIOperation operation;
    enum MPIStatus status;
};


/* @section: Structs for keeping internal state */

/**
 * Represents an MPI connection.
 * This includes the source/destination rank and port:
 *  1) rank is the MPI rank;
 *  2) port is used to port over TCP transparently.
 */
struct MPIConnection {
    int src_rank;
    int dst_rank;
    uint16_t src_port;
    uint16_t dst_port;

    bool operator<(const MPIConnection &other) const {
        if (src_rank < other.src_rank)
            return true;
        if (src_rank > other.src_rank)
            return false;
        if (dst_rank < other.dst_rank)
            return true;
        if (dst_rank > other.dst_rank)
            return false;
        if (src_port < other.src_port)
            return true;
        if (src_port > other.src_port)
            return false;
        if (dst_port < other.dst_port)
            return true;
        if (dst_port > other.dst_port)
            return false;
        return false;
    }
};

#endif // DCCS_MESSAGE_H

