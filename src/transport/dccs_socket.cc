// Custom socket operations that transmit data over MPI daemon

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <dlfcn.h>
#include <stdio.h>

#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <map>
#include <string>

#include "lib/logging.h"
#include "dccs_config.h"
#include "dccs_message.h"
#include "dccs_utils.h"

using namespace std;

// payload length for IPC response
#define IPC_COMMAND_BUFLEN 32

int ipc_socket = -1;


/* @section: IPC helper functions */

/**
 * Send the given buffer through the IPC socket.
 * @return 0 if successful, or -1 otherwise.
 */
int ipc_send_raw(const char *buf, size_t length) {
    int sd = ipc_socket;
    int rv;
    ssize_t bytes_sent;
    size_t total_sent = 0;
    struct sockaddr_un serveraddr;

    // A do/while(false) loop is used to make error cleanup easier.
    do {
        if (sd != -1)
            goto socket_connected;

        sd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sd < 0) {
            log_perror("IPC send raw: socket()");
            break;
        }

        memset(&serveraddr, 0, sizeof serveraddr);
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, SERVER_PATH);

        rv = connect(sd, (struct sockaddr *)&serveraddr,
                        (socklen_t)SUN_LEN(&serveraddr));
        if (rv < 0) {
            log_perror("IPC send raw: connect()");
            break;
        }

        ipc_socket = sd;

socket_connected:
        while (total_sent < length) {
            bytes_sent = send(sd, buf, length, 0);
            if (bytes_sent < 0) {
                log_perror("IPC send raw: send()");
                break;
            }

            total_sent += (size_t)bytes_sent;
        }

        return 0;
    } while (false);

    return -1;
}

/**
 * Receive data into the given buffer through the IPC socket.
 * @return 0 if successful, or -1 otherwise.
 */
int ipc_recv_raw(char *buf, size_t length) {
    int sd = ipc_socket;
    int rv;
    ssize_t bytes_recv;
    size_t total_recv = 0;
    struct sockaddr_un serveraddr;

    // A do/while(false) loop is used to make error cleanup easier.
    do {
        if (sd != -1)
            goto socket_connected;

        sd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (sd < 0) {
            log_perror("IPC send raw: socket()");
            break;
        }

        memset(&serveraddr, 0, sizeof serveraddr);
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, SERVER_PATH);

        rv = connect(sd, (struct sockaddr *)&serveraddr,
                        (socklen_t)SUN_LEN(&serveraddr));
        if (rv < 0) {
            log_perror("IPC send raw: connect()");
            break;
        }

        ipc_socket = sd;

socket_connected:
        while (total_recv < length) {
            bytes_recv = recv(sd, buf, length, 0);
            if (bytes_recv < 0) {
                log_perror("IPC recv raw: recv()");
                break;
            }

            total_recv += (size_t)bytes_recv;
        }

        return 0;
    } while (false);

    return -1;
}

int ipc_send_command(struct IPCRequest *header, const char *payload) {
    char *buf = new char[sizeof *header + header->length];
    memcpy(buf, header, sizeof *header);
    memcpy(buf + sizeof *header, payload, header->length);
    int rv = ipc_send_raw(buf, sizeof *header + header->length);
    delete[] buf;
    return rv;
}

int ipc_recv_response(struct IPCResponse *header, char *buf, size_t length) {
    int rv;

    if ((rv = ipc_recv_raw((char *)header, sizeof *header)) != 0) {
        log_error("Failed to receive IPC response header.\n");
        return -1;
    }

    if (header->length != 0) {
        if (header->length > length)
            log_warning("IPC response payload is longer the given buffer.\n");

        if ((rv = ipc_recv_raw(buf, min(header->length, length))) != 0) {
            log_error("Failed to receive IPC response payload.\n");
            return -1;
        }
    }

    return 0;
}


// Global map tracking dependencies
map<tuple<struct sockaddr *, socklen_t>,
    tuple<string,string>> sockaddr_to_hostport;

// Function pointers to the original glibc functions.

static int (*real_getaddrinfo)(const char *, const char *,
    const struct addrinfo *, struct addrinfo **) = NULL;
static void (*real_freeaddrinfo)(struct addrinfo *) = NULL;
static int (*real_socket)(int, int, int) = NULL;
//static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;
static ssize_t (*real_send)(int, const void *, size_t, int) = NULL;
static ssize_t (*real_recv)(int, void *, size_t, int) = NULL;

/* @section: Override syscalls */

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
    if (real_getaddrinfo == NULL) {
        *(void **)(&real_getaddrinfo) = dlsym(RTLD_NEXT, "getaddrinfo");
    }

    int rv;
    struct addrinfo *p;
    rv = real_getaddrinfo(node, service, hints, res);
    string hostname = string(node);
    string port = string(service);
    for (p = *res; p != NULL; p = p->ai_next) {
        sockaddr_to_hostport[make_tuple(p->ai_addr, p->ai_addrlen)] =
            make_tuple(hostname, port);
    }

    return rv;
}

void freeaddrinfo(struct addrinfo *res) {
    if (real_freeaddrinfo == NULL) {
        *(void **)(&real_freeaddrinfo) = dlsym(RTLD_NEXT, "freeaddrinfo");
    }

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sockaddr_to_hostport.erase(make_tuple(p->ai_addr, p->ai_addrlen));
    }

    real_freeaddrinfo(res);
}

int socket(int domain, int type, int protocol) {
    if (real_socket == NULL) {
        *(void **)(&real_socket) = dlsym(RTLD_NEXT, "socket");
    }

    int rv;
    rv = real_socket(domain, type, protocol);
    return rv;
}


int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    int rv;

    log_debug("syscall connect(): sockfd = %d.\n", sockfd);

    struct sockaddr *addr_copy = (struct sockaddr *)addr;
    auto it = sockaddr_to_hostport.find(make_tuple(addr_copy, addrlen));
    if (it == sockaddr_to_hostport.end()) {
        log_error("syscall connect(): sockaddr not found.\n");
        return -1;
    }

    tuple<string, string> t = it->second;
    string hostname = get<0>(t);
    string port_str = get<1>(t);
    short port = (short)stoi(port_str);

    struct IPCRequest request;
    memset(&request, 0, sizeof request);
    request.fd = sockfd;
    //request.port = port;
    request.operation = IPCOperation::CONNECT;
    request.length = hostname.length() + 1 + sizeof port;

    char *payload = new char[request.length];
    memcpy(payload, hostname.c_str(), hostname.length() + 1);
    memcpy(payload + hostname.length() + 1, &port, sizeof port);

    rv = ipc_send_command(&request, payload);
    delete[] payload;
    errif_return(rv, "Failed to send IPC command for connect().\n");

    struct IPCResponse response;
    memset(&response, 0, sizeof response);
    char recvbuf[IPC_COMMAND_BUFLEN];
    rv = ipc_recv_response(&response, recvbuf, IPC_COMMAND_BUFLEN);
    errif_return(rv, "Failed to recv IPC response for connect().\n");

    // Process response payload (optional).

    log_debug("syscall connect(): sockfd = %d returned %d.\n", sockfd,
                response.retval_int);
    return response.retval_int;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    printf("send: sockfd = %d, buf = %p, len = %zu, flags = %d.\n",
        sockfd, buf, len, flags);
    if (real_send == NULL) {
        *(void **)(&real_send) = dlsym(RTLD_NEXT, "send");
    }

    return real_send(sockfd, buf, len, flags);
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    printf("recv: sockfd = %d, buf = %p, len = %zu, flags = %d.\n",
        sockfd, buf, len, flags);
    if (real_recv == NULL) {
        *(void **)(&real_recv) = dlsym(RTLD_NEXT, "recv");
    }

    return real_recv(sockfd, buf, len, flags);
}

