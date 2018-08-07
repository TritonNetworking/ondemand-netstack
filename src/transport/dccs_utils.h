/**
 * Common utility functions.
 */

#ifndef DCCS_UTIL_H
#define DCCS_UTIL_H

#include <byteswap.h>
#include <map>
#include <string>

#include <netinet/in.h>
#include <sys/socket.h>

#include "dccs_message.h"

using namespace std;

#define USE_C_ERROR_HANDLE_SHORTCUT 1


/* @section: C-error easy handling macros */
#if USE_C_ERROR_HANDLE_SHORTCUT

#define errif(rv, ...) \
    if ((rv) < 0) { \
        log_error(__VA_ARGS__); \
    }
#define errif_return(rv, ...) \
    if ((rv) < 0) { \
        log_error(__VA_ARGS__); \
        return -1; \
    }
#define perrif_return(rv, ...) \
    if ((rv) < 0) { \
        log_perror(__VA_ARGS__); \
        return -1; \
    }
#define errif_break(rv, ...) \
    if ((rv) < 0) { \
        log_error(__VA_ARGS__); \
        break; \
    }
#define perrif_break(rv, ...) \
    if ((rv) < 0) { \
        log_perror(__VA_ARGS__); \
        break; \
    }
/*
#define errif(rv, err_msg, ...) \
    if ((rv) < 0) { \
        log_error(err_msg, __VA_ARGS__); \
    }
#define errif_return(rv, err_msg, ...) \
    if ((rv) < 0) { \
        log_error(err_msg, __VA_ARGS__); \
        return -1; \
    }
#define perrif_return(rv, err_msg, ...) \
    if ((rv) < 0) { \
        log_perror(err_msg, __VA_ARGS__); \
        return -1; \
    }
#define errif_break(rv, err_msg, ...) \
    if ((rv) < 0) { \
        log_error(err_msg, __VA_ARGS__); \
        break; \
    }
#define perrif_break(rv, err_msg, ...) \
    if ((rv) < 0) { \
        log_perror(err_msg, __VA_ARGS__); \
        break; \
    }
 */

#endif


/* @section: Network utility functions */

#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint64_t htonll(uint64_t x) { return bswap_64(x); }
static inline uint64_t ntohll(uint64_t x) { return bswap_64(x); }
#elif __BYTE_ORDER == __BIG_ENDIAN
static inline uint64_t htonll(uint64_t x) { return x; }
static inline uint64_t ntohll(uint64_t x) { return x; }
#else
#error __BYTE_ORDER is neither __LITTLE_ENDIAN nor __BIG_ENDIAN
#endif

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

uint16_t allocate_ephemeral_port() {
    // TODO: implement this.
    return -1;
}

static bool EOF_IGNORE;

/**
 * Send the entire buffer.
 * @return 0 if successful, -1 otherwise.
 */
int send_all(int sockfd, const void *buf, size_t len, int flags) {
    ssize_t bytes_sent;
    size_t total_sent = 0;

    while (total_sent < len) {
        char *curr = (char *)buf + total_sent;
#if USE_REAL_SOCKET
    #define SEND real_send
#else
    #define SEND send
#endif
        bytes_sent = SEND(sockfd, curr, len - total_sent, flags);
#undef SEND
        if (bytes_sent < 0) {
            log_perror("send all: send()");
            return -1;
        }

        total_sent += (size_t)bytes_sent;
    }

    return 0;
}

/**
 * Receive the entire buffer.
 * @return 0 if successful, -1 otherwise.
 */
int recv_all(int sockfd, void *buf, size_t len, int flags,
                bool &eof = EOF_IGNORE) {
    ssize_t bytes_recv;
    size_t total_recv = 0;

    eof = false;
    while (total_recv < len) {
        char *curr = (char *)buf + total_recv;
#if USE_REAL_SOCKET
    #define RECV real_recv
#else
    #define RECV recv
#endif
        bytes_recv = RECV(sockfd, curr, len - total_recv, flags);
#undef RECV
        if (bytes_recv < 0) {
            log_perror("recv all: recv()");
            return -1;
        } else if (bytes_recv == 0) {
            eof = true;
            return -1;
        }

        total_recv += (size_t)bytes_recv;
    }

    return 0;
}

/**
 * Send the entire struct.
 */
template<class T>
int send(int sockfd, const T &t, int flags) {
    const void *buf = (const void *)&t;
    size_t len = sizeof(T);
    return send_all(sockfd, buf, len, flags);
}

/**
 * Receive the entire struct.
 */
template<class T>
int recv(int sockfd, T &t, int flags, bool &eof = EOF_IGNORE) {
    void *buf = (void *)&t;
    size_t len = sizeof(T);
    return recv_all(sockfd, buf, len, flags, eof);
}


/* @section: String helpers */

string to_string(IPCOperation operation) {
    static map<IPCOperation, string> m;
    if (m.size() == 0) {
#define INSERT_ELEMENT(p) m[p] = std::string(#p)
        INSERT_ELEMENT(IPCOperation::NOP);
        INSERT_ELEMENT(IPCOperation::GETADDRINFO);
        INSERT_ELEMENT(IPCOperation::CONNECT);
        INSERT_ELEMENT(IPCOperation::SOCKET);
        INSERT_ELEMENT(IPCOperation::CLOSE);
#undef INSERT_ELEMENT
    }

    return m[operation];
}

const char *to_c_str(IPCOperation operation) {
    return to_string(operation).c_str();
}

string to_string(MPIOperation operation) {
    static map<MPIOperation, string> m;
    if (m.size() == 0) {
#define INSERT_ELEMENT(p) m[p] = std::string(#p)
        INSERT_ELEMENT(MPIOperation::NOP);
        INSERT_ELEMENT(MPIOperation::CONNECT);
#undef INSERT_ELEMENT
    }

    return m[operation];
}

const char *to_c_str(MPIOperation operation) {
    return to_string(operation).c_str();
}

string to_string(MPIStatus status) {
    static map<MPIStatus, string> m;
    if (m.size() == 0) {
#define INSERT_ELEMENT(p) m[p] = std::string(#p)
        INSERT_ELEMENT(MPIStatus::SUCCESS);
        INSERT_ELEMENT(MPIStatus::INVAL);
        INSERT_ELEMENT(MPIStatus::CONNREFUSED);
#undef INSERT_ELEMENT
    }

    return m[status];
}

const char *to_c_str(MPIStatus status) {
    return to_string(status).c_str();
}


/* @section: Shared memory */

void *allocate_shm(size_t length) {
    // TODO: not implemented.
    log_warning("Not implemented\n");
    return NULL;
}

#endif // DCCS_UTIL_H

