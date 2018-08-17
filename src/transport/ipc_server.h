#ifndef IPC_SERVER_H
#define IPC_SERVER_H

#include <errno.h>
#include <inttypes.h>
#include <mpi.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <map>
#include <vector>
#include <thread>
#include <mutex>
#include <utility>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "dccs_config.h"
#include "lib/logging.h"
#include "dccs_message.h"
#include "dccs_utils.h"

#include "connection_manager.h"
#include "mpi_server.h"

using namespace std;

/**
 * IPC server listens to all IPC request from custom socket library,
 *  and contacts the MPI daemon to fulfill the request.
 */
class IPCServer {
private:
    volatile bool signaled;

    // List of all IPC handler threads.
    vector<thread> ipc_threads;
    mutex ipc_threads_mutex;

    MPIServer * const mpid;
    ConnectionManager * const connmgr;

    void AddThread(thread &t);
    void RemoveThread(thread::id tid);

    int SendResponse(int ipcsd, IPCResponse response);

    void ThreadIPCHandler(int sd);
    int Listen();

    int Connect(int ipcsd, int fd, const char *hostname, uint16_t dst_port);
    int Close(int ipcsd, int fd);
    int Bind(int ipcsd, int fd, uint8_t port);

public:
    IPCServer(MPIServer *mpid);

    int Start();
    void Stop();
};


void IPCServer::AddThread(thread &t) {
    ipc_threads_mutex.lock();
    ipc_threads.push_back(move(t));
    ipc_threads_mutex.unlock();
}

void IPCServer::RemoveThread(thread::id tid) {
    ipc_threads_mutex.lock();
    ipc_threads.erase(
        std::remove_if(ipc_threads.begin(), ipc_threads.end(),
            [&] (auto &t) { return t.get_id() == tid; }),
        ipc_threads.end());
    ipc_threads_mutex.unlock();
}

int IPCServer::SendResponse(int ipcsd, IPCResponse response) {
    log_verbose("ipc | Sending response to %d ...\n", ipcsd);
    int rv = send(ipcsd, response, 0);
    perrif_return(rv, "ipc | send()");
    log_verbose("ipc | Sent response to %d.\n", ipcsd);
    return 0;
}

void IPCServer::ThreadIPCHandler(int sd) {
    int rv;
    bool eof;
    struct IPCRequest request;
    char *buf = NULL;
    bool keep_listening = true;

    log_verbose("ipc_handler | Start\n");

    do {
        rv = recv(sd, request, 0, eof);
        if (eof) {
            keep_listening = false;
            break;
        }

        perrif_break(rv, "ipc_handler | recv()");

        log_info("ipc_handler | Received IPC request from sd %d: "
                "fd = %d, operation = %s, payload length = %zu.\n",
                sd, request.fd, to_c_str(request.operation), request.length);

        if (request.length > 0) {
            buf = new char[request.length];
            rv = recv_all(sd, buf, request.length, 0);
            perrif_break(rv, "ipc_handler | process ipc payload: recv()");
        }

        switch (request.operation) {
            case IPCOperation::CONNECT: {
                const char *hostname = buf;
                uint16_t port = *((uint16_t *)(buf + strlen(buf) + 1));
                rv = Connect(sd, request.fd, hostname, port);
                errif_break(rv, "ipc_handler | Failed to handle %d:"
                            "connect(%d, %s, %" PRIu16 ")\n",
                            sd, request.fd, hostname, port);
                break;
            }
            case IPCOperation::CLOSE: {
                rv = Close(sd, request.fd);
                errif_break(rv, "ipc_handler | Failed to handle %d:close(%d)\n",
                                sd, request.fd);
                keep_listening = false;
            }
            default: {
                log_warning("ipc_handler | Unhandled IPC operation %s.\n",
                                to_c_str(request.operation));
                break;
            }
        }

        if (buf != NULL) {
            delete[] buf;
            buf = NULL;
        }

        log_info("ipc_handler | Done with IPC request.\n");
    } while (keep_listening);

    if (keep_listening)
        log_warning("ipc_handler | Unexpected close of IPC socket %d.\n", sd);

    close(sd);

    log_verbose("ipc_handler | End\n");
    RemoveThread(this_thread::get_id());
}

/**
 * Main listen loop that awaits application requests.
 * This implements IPC using UNIX domain socket, and exchanges:
 *  1) shared memory file descriptor, which holds the send/recv buffer;
 *  2) verb (send/recv) and arguments.
 */
int IPCServer::Listen() {
    int listen_sd = -1, sd = -1;
    int rv;
    struct sockaddr_un serveraddr;

    log_verbose("ipc_loop | IPC server started ...\n");
    // A do/while(false) loop is used to make error cleanup easier.
    do {
        listen_sd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_sd < 0) {
            log_perror("ipc_loop | socket");
            break;
        }

        memset(&serveraddr, 0, sizeof(serveraddr));
        serveraddr.sun_family = AF_UNIX;
        strcpy(serveraddr.sun_path, SERVER_PATH);

        rv = bind(listen_sd, (struct sockaddr *)&serveraddr,
                    (socklen_t)SUN_LEN(&serveraddr));
        if (rv < 0) {
            log_perror("ipc_loop | bind");
            break;
        }

        rv = listen(listen_sd, 10);
        if (rv < 0) {
            log_perror("ipc_loop | listen");
            break;
        }

        log_info("ipc_loop | Waiting for client connections ...\n");

        while (!signaled) {
            sd = accept(listen_sd, NULL, NULL);
            if (sd < 0) {
                log_perror("ipc_loop | accept");
                sd = -1;
                continue;
            }

            thread t(&IPCServer::ThreadIPCHandler, this, sd);
            errif(rv, "ipc_loop | Failed to create IPC handler thread.\n");
            t.detach();
            AddThread(t);
        }

        // Optional clean up

        // Program complete
    } while (false);

    if (listen_sd != -1)
        close(listen_sd);

    unlink(SERVER_PATH);
    log_verbose("ipc_loop | IPC server stopped.\n");
    return 0;
}

/**
 * Initiates a connection on a socket.
 * This will contact the probe server through MPI.
 * @return 0 if successful; -1 otherwise.
 */
int IPCServer::Connect(int ipcsd, int fd, const char *hostname, uint16_t dst_port) {
    int dst_rank;
    int rv;
    struct IPCResponse ipc_response = {};

    log_verbose("ipc_connect | Start\n");
    dst_rank = mpid->GetRank(string(hostname));
    if (dst_rank == -1) {
        log_error("Host %s not found.\n", hostname);
        ipc_response.retval_int = -1;
        ipc_response.error = ECONNREFUSED;
        rv = SendResponse(ipcsd, ipc_response);
        return rv;
    }

    uint16_t src_port = connmgr->AllocateEphemeralPort();
    //???[fd_key(ipcsd, fd)] = src_port;

    MPIConnection connection = {};
    connection.src_rank = mpi_rank;
    connection.dst_rank = dst_rank;
    connection.src_port = src_port;
    connection.dst_port = dst_port;
    connmgr->AddConnection(ipcsd, fd, connection);

    struct MPIRequest request = {};
    request.src_port = src_port;
    request.dst_port = dst_port;
    request.operation = MPIOperation::CONNECT;

    log_verbose("ipc_connect | Sending MPI request %s to %d:%hd...\n",
                    to_c_str(request.operation), dst_rank, dst_port);
    rv = MPI_Send(&request, 1, MPI_struct_request, dst_rank, MPI_TAG_REQUEST, MPI_COMM_WORLD);
    errif_return(rv, "ipc_connect | MPI_Send failed\n");
    log_verbose("ipc_connect | MPI request sent.\n");

    log_verbose("ipc_connect | Waiting for MPI response ...\n");
    struct MPIResponse response = {};
    rv = MPI_Recv(&response, 1, MPI_struct_response, dst_rank, MPI_TAG_RESPONSE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    errif_return(rv, "ipc_connect | MPI_Recv failed\n");
    log_verbose("ipc_connect | Received MPI response %s.\n",
                    to_c_str(response.status));

    switch (response.status) {
        case MPIStatus::SUCCESS: {
            ipc_response.retval_int = 0;
            ipc_response.error = 0;
            break;
        }
        case MPIStatus::CONNREFUSED: {
            ipc_response.retval_int = -1;
            ipc_response.error = ECONNREFUSED;
            break;
        }
        case MPIStatus::PERM: {
            ipc_response.retval_int = -1;
            ipc_response.error = EPERM;
            break;
        }
        default: {
            log_error("ipc_connect | Unhandled MPI response %s.\n",
                        to_c_str(response.status));
            ipc_response.retval_int = -1;
            ipc_response.error = EINVAL;
            break;
        }
    }

    ipc_response.length = 0;

    rv = SendResponse(ipcsd, ipc_response);

    log_verbose("ipc_connect | End\n");
    return rv;
}

int IPCServer::Close(int ipcsd, int fd) {
    log_verbose("ipc_close | Start\n");

    // TODO: inform the remote peer?
    bool success = connmgr->RemoveConnection(ipcsd, fd);
    log_info("ipc_close | Removed %zu items from fd table.\n", success ? 1 : 0);

    struct IPCResponse response = {};
    response.retval_int = 0;
    response.error = 0;

    int rv = SendResponse(ipcsd, response);

    log_verbose("ipc_close | End\n");
    return rv;
}

int IPCServer::Bind(int ipcsd, int fd, uint8_t port) {
    log_verbose("ipc_bind | Start\n");

    bool success = connmgr->Bind(ipcsd, fd, port);
    struct IPCResponse response = {};
    if (success) {
        response.retval_int = 0;
        response.error = 0;
    } else {
        log_info("IPC bind(%d, %d, %" PRIu8 "): port in use.\n");
        response.retval_int = -1;
        response.error = EADDRINUSE;
    }

    int rv = SendResponse(ipcsd, response);

    log_verbose("ipc_bind | End\n");
    return rv;
}

int Send() {
    // TODO: implement this
    return -1;
}

int Recv() {
    // TODO: implement this
    return -1;
}

IPCServer::IPCServer(MPIServer *mpid)
        : mpid(mpid),
          connmgr(new ConnectionManager()) {
    signaled = false;
}

int IPCServer::Start() {
    return Listen();
}

void IPCServer::Stop() {
    signaled = true;
    // TODO:
    //  1. Wait for all threads
    //  2. Shutdown down connection manager
}

#endif // IPC_SERVER_H

