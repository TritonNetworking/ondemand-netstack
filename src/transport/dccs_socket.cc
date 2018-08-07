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

/* @section: function pointers to original glibc socket functions. */

#define USE_REAL_SOCKET 1

static bool real_socket_function_init = false;
static int (*real_getaddrinfo)(const char *, const char *,
    const struct addrinfo *, struct addrinfo **) = NULL;
static void (*real_freeaddrinfo)(struct addrinfo *) = NULL;
static int (*real_socket)(int, int, int) = NULL;
static int (*real_connect)(int, const struct sockaddr *, socklen_t) = NULL;
static ssize_t (*real_send)(int, const void *, size_t, int) = NULL;
static ssize_t (*real_recv)(int, void *, size_t, int) = NULL;

#include "dccs_config.h"
#include "lib/logging.h"
#include "dccs_message.h"
#include "dccs_utils.h"

using namespace std;

// payload length for IPC response
#define IPC_COMMAND_BUFLEN 32

int ipc_socket = -1;


/* @section: function pointers to original glibc socket functions. */

void load_real_socket_functions() {
    if (real_socket_function_init)
        return;

    *(void **)(&real_getaddrinfo) = dlsym(RTLD_NEXT, "getaddrinfo");
    *(void **)(&real_freeaddrinfo) = dlsym(RTLD_NEXT, "freeaddrinfo");
    *(void **)(&real_socket) = dlsym(RTLD_NEXT, "socket");
    *(void **)(&real_connect) = dlsym(RTLD_NEXT, "connect");
    *(void **)(&real_send) = dlsym(RTLD_NEXT, "send");
    *(void **)(&real_recv) = dlsym(RTLD_NEXT, "recv");

    real_socket_function_init = true;
}


/* @section: IPC helper functions */

/**
 * Send the given buffer through the IPC socket.
 * @return 0 if successful, or -1 otherwise.
 */
int ipc_send_raw(const char *buf, size_t length) {
    int sd = ipc_socket;
    int rv;
    struct sockaddr_un serveraddr;

    log_verbose("ipc_send_raw | buf = %p, length = %zu.\n", buf, length);

    // A do/while(false) loop is used to make error cleanup easier.
    do {
        if (sd != -1)
            goto socket_connected;

        sd = real_socket(AF_UNIX, SOCK_STREAM, 0);
        if (sd < 0) {
            log_perror("IPC send raw: socket()");
            break;
        }

        memset(&serveraddr, 0, sizeof serveraddr);
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, SERVER_PATH);

        rv = real_connect(sd, (struct sockaddr *)&serveraddr,
                        (socklen_t)SUN_LEN(&serveraddr));
        if (rv < 0) {
            log_perror("IPC send raw: connect()");
            break;
        }

        ipc_socket = sd;

socket_connected:
        rv = send_all(sd, buf, length, 0);
        perrif_break(rv, "IPC send raw: send()");

        log_verbose("ipc_send_raw | return 0\n");
        return 0;
    } while (false);

    log_verbose("ipc_send_raw | return -1\n");
    return -1;
}

/**
 * Receive data into the given buffer through the IPC socket.
 * @return 0 if successful, or -1 otherwise.
 */
int ipc_recv_raw(char *buf, size_t length) {
    int sd = ipc_socket;
    int rv;
    struct sockaddr_un serveraddr;

    log_verbose("ipc_recv_raw | buf = %p, length = %zu.\n", buf, length);

    // A do/while(false) loop is used to make error cleanup easier.
    do {
        if (sd != -1)
            goto socket_connected;

        sd = real_socket(AF_UNIX, SOCK_STREAM, 0);
        if (sd < 0) {
            log_perror("IPC send raw: socket()");
            break;
        }

        memset(&serveraddr, 0, sizeof serveraddr);
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, SERVER_PATH);

        rv = real_connect(sd, (struct sockaddr *)&serveraddr,
                        (socklen_t)SUN_LEN(&serveraddr));
        if (rv < 0) {
            log_perror("IPC send raw: connect()");
            break;
        }

        ipc_socket = sd;

socket_connected:
        rv = recv_all(sd, buf, length, 0);
        perrif_break(rv, "IPC recv raw: recv()");

        log_verbose("ipc_recv_raw | return 0\n");
        return 0;
    } while (false);

    log_verbose("ipc_recv_raw | return -1\n");
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


/* @section: Override syscalls */

// Global map tracking dependencies
map<tuple<struct sockaddr *, socklen_t>,
    tuple<string,string>> sockaddr_to_hostport;

int getaddrinfo(const char *node, const char *service,
                const struct addrinfo *hints, struct addrinfo **res) {
    load_real_socket_functions();

    log_verbose("custom socket | getaddrinfo(%s, %s, %p, %p)\n",
                    node, service, hints, res);

    int rv;
    struct addrinfo *p;
    rv = real_getaddrinfo(node, service, hints, res);
    string hostname = string(node);
    string port = string(service);
    for (p = *res; p != NULL; p = p->ai_next) {
        sockaddr_to_hostport[make_tuple(p->ai_addr, p->ai_addrlen)] =
            make_tuple(hostname, port);
    }

    log_verbose("custom socket | getaddrinfo(%s, %s, %p, %p) return %d\n",
                    node, service, hints, res, rv);
    return rv;
}

void freeaddrinfo(struct addrinfo *res) {
    load_real_socket_functions();

    log_verbose("custom socket | freeaddrinfo(%p)\n", res);

    struct addrinfo *p;
    for (p = res; p != NULL; p = p->ai_next) {
        sockaddr_to_hostport.erase(make_tuple(p->ai_addr, p->ai_addrlen));
    }

    real_freeaddrinfo(res);
    log_verbose("custom socket | freeaddrinfo(%p) returned\n", res);
}

int socket(int domain, int type, int protocol) {
    load_real_socket_functions();

    log_verbose("custom socket | socket(%d, %d, %p)\n",
                    domain, type, protocol);

    int rv;
    rv = real_socket(domain, type, protocol);
    log_verbose("custom socket | socket(%d, %d, %p) returned %d\n",
                    domain, type, protocol, rv);
    return rv;
}


int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    load_real_socket_functions();

    int rv;

    log_verbose("custom socket | connect(%d, %p, %zu)\n",
                    sockfd, addr, addrlen);

    struct sockaddr *addr_copy = (struct sockaddr *)addr;
    auto it = sockaddr_to_hostport.find(make_tuple(addr_copy, addrlen));
    if (it == sockaddr_to_hostport.end()) {
        log_error("connect: sockaddr not found.\n");
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
    errif_return(rv, "connect | Failed to send IPC command for connect().\n");

    struct IPCResponse response;
    memset(&response, 0, sizeof response);
    char recvbuf[IPC_COMMAND_BUFLEN];
    rv = ipc_recv_response(&response, recvbuf, IPC_COMMAND_BUFLEN);
    errif_return(rv, "connect | Failed to recv IPC response for connect().\n");

    // Process response payload (optional).
    if (response.retval_int != 0)
        errno = response.error;

    log_verbose("custom socket | connect(%d, %p, %zu) returned %d\n",
                    sockfd, addr, addrlen, response.retval_int);
    return response.retval_int;
}

ssize_t send(int sockfd, const void *buf, size_t len, int flags) {
    load_real_socket_functions();

    log_verbose("custom socket | send(%d, %p, %zu, %d)\n",
                    sockfd, buf, len, flags);

    ssize_t numbytes = real_send(sockfd, buf, len, flags);

    log_verbose("custom socket | send(%d, %p, %zu, %d) returned %d\n",
                    sockfd, buf, len, flags, numbytes);
    return numbytes;
}

ssize_t recv(int sockfd, void *buf, size_t len, int flags) {
    load_real_socket_functions();

    log_verbose("custom socket | recv(%d, %p, %zu, %d)\n",
                    sockfd, buf, len, flags);

    ssize_t numbytes = real_recv(sockfd, buf, len, flags);

    log_verbose("custom socket | recv(%d, %p, %zu, %d) returned %d\n",
                    sockfd, buf, len, flags, numbytes);
    return numbytes;
}

