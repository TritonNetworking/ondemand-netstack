#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <errno.h>
#include <inttypes.h>
#include <mpi.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

using namespace std;

/**
 * Thread-safe manager of connections and process file descriptor mapping.
 */
class ConnectionManager {
private:
    typedef std::pair<int, int> procfd_t;

    map<procfd_t, MPIConnection> procfd_to_connection;
    map<MPIConnection, procfd_t> connection_to_procfd;
    mutex procfd_and_connection_mutex;

    // Mapping of bind port to IPC sd and process-specific fd
    map<uint8_t, procfd_t> bind_port_to_procfd;
    map<procfd_t, uint8_t> procfd_to_bind_port;
    mutex bind_port_and_procfd_mutex;

/*
    static size_t encode_key(int proc, int fd) {
        return (size_t)proc << 32 | (unsigned int)fd;
    }
 */

public:
    ConnectionManager();

    uint16_t AllocateEphemeralPort();

    bool Bind(int proc, int fd, uint8_t port);
    void Unbind(int proc, int fd);
    std::tuple<int, int> LookupBindListener(uint8_t port);
    void AddConnection(int proc, int fd, MPIConnection connection);
    bool RemoveConnection(int proc, int fd);
    bool RemoveConnection(MPIConnection connection);
    MPIConnection GetConnection(int proc, int fd);
    std::tuple<int, int> GetProcFd(MPIConnection connection);
};

ConnectionManager::ConnectionManager() {
}

uint16_t ConnectionManager::AllocateEphemeralPort() {
    // TODO: implement this
    return -1;
}

bool ConnectionManager::Bind(int proc, int fd, uint8_t port) {
    bool success;
    auto procfd = std::make_pair(proc, fd);
    bind_port_and_procfd_mutex.lock();
    if (bind_port_to_procfd.find(port) != bind_port_to_procfd.end() ||
        procfd_to_bind_port.find(procfd) != procfd_to_bind_port.end()) {
        success = false;
    } else {
        bind_port_to_procfd[port] == procfd;
        procfd_to_bind_port[procfd] = port;
    }
    bind_port_and_procfd_mutex.unlock();
    return success;
}

void ConnectionManager::Unbind(int proc, int fd) {
    bind_port_and_procfd_mutex.lock();
    auto procfd = std::make_pair(proc, fd);
    auto it = procfd_to_bind_port.find(procfd);
    if (it != procfd_to_bind_port.end()) {
        bind_port_to_procfd.erase(it->second);
        procfd_to_bind_port.erase(it);
    }

    bind_port_and_procfd_mutex.unlock();
}

std::tuple<int, int> ConnectionManager::LookupBindListener(uint8_t port) {
    procfd_t procfd;
    bind_port_and_procfd_mutex.lock();
    auto it = bind_port_to_procfd.find(port);
    if (it != bind_port_to_procfd.end())
        procfd = it->second;
    else
        procfd = std::make_pair(-1, -1);
    bind_port_and_procfd_mutex.unlock();
    return procfd;
}

void ConnectionManager::AddConnection(int proc, int fd, MPIConnection connection) {
    procfd_and_connection_mutex.lock();
    auto procfd = std::make_pair(proc, fd);
    procfd_to_connection[procfd] = connection;
    connection_to_procfd[connection] = procfd;
    procfd_and_connection_mutex.unlock();
}

bool ConnectionManager::RemoveConnection(int proc, int fd) {
    bool removed;
    procfd_and_connection_mutex.lock();
    auto it = procfd_to_connection.find(std::make_pair(proc, fd));
    if (it == procfd_to_connection.end()) {
        removed = false;
    } else {
        connection_to_procfd.erase(it->second);
        procfd_to_connection.erase(it);
        removed = true;
    }
    procfd_and_connection_mutex.unlock();
    return removed;
}

bool ConnectionManager::RemoveConnection(MPIConnection connection) {
    bool removed;
    procfd_and_connection_mutex.lock();
    auto it = connection_to_procfd.find(connection);
    if (it == connection_to_procfd.end()) {
        removed = false;
    } else {
        procfd_to_connection.erase(it->second);
        connection_to_procfd.erase(it);
        removed = true;
    }
    procfd_and_connection_mutex.unlock();
    return removed;
}

MPIConnection ConnectionManager::GetConnection(int proc, int fd) {
    procfd_and_connection_mutex.lock();
    auto connection = procfd_to_connection[std::make_pair(proc, fd)];
    procfd_and_connection_mutex.unlock();
    return connection;
}

std::tuple<int, int> ConnectionManager::GetProcFd(MPIConnection connection) {
    procfd_and_connection_mutex.lock();
    auto procfd = connection_to_procfd[connection];
    procfd_and_connection_mutex.unlock();
    return procfd;
}

#endif // CONNECTION_MANAGER_H

