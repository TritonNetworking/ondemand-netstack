#ifndef MPI_SERVER_H
#define MPI_SERVER_H

#include <limits.h>
#include <mpi.h>
#include <stdlib.h>
#include <unistd.h>

#include <map>
#include <mutex>
#include <thread>

#include "lib/logging.h"
#include "lib/dccs_utils.h"

extern MPI_Datatype MPI_struct_request, MPI_struct_response;

/**
 * MPI server listens and responds to MPI requests sent from IPC server.
 */
class MPIServer {
private:
    int size, rank;

    // The network ID that uniquely identify this MPI network
    int mpi_network_id;

    thread ts;

    map<string, int> hostname_to_rank;
    mutex hostname_to_rank_mutex;

    int InitializeNetwork();
    int InitializeHostnameRankMapping();

    int StartServerThread();
    void DaemonThread();

public:
    static int InitializeMPIDataTypes();

    MPIServer(int size, int rank);

    int Start();
    void Stop();

    int GetRank(string hostname);

    MPIResponse Connect(MPIRequest request);
};

// Register structs as MPI data types.
int MPIServer::InitializeMPIDataTypes() {
    int rv;

    log_verbose("Initializing MPI struct types ...\n");

    // Initialize MPI struct for MPIRequest
    {
        const int numitems = 3;
        int blocklengths[numitems] = { 1, 1, 1 };
        MPI_Datatype types[numitems] = {
            MPI_UINT16_T,
            MPI_UINT16_T,
            MPI_UINT8_T,
        };
        MPI_Aint offsets[numitems] = {
            offsetof(struct MPIRequest, src_port),
            offsetof(struct MPIRequest, dst_port),
            offsetof(struct MPIRequest, operation),
        };
        rv = MPI_Type_create_struct(numitems, blocklengths, offsets, types,
                                    &MPI_struct_request);
        errif_return(rv, "Failed to create MPI struct MPIRequest.\n");
        rv = MPI_Type_commit(&MPI_struct_request);
        errif_return(rv, "Failed to commit MPI struct mpi_req_hader.\n");
    }

    // Initialize MPI struct for MPIResponse
    {
        const int numitems = 4;
        int blocklengths[numitems] = { 1, 1, 1, 1 };
        MPI_Datatype types[numitems] = {
            MPI_UINT16_T,
            MPI_UINT16_T,
            MPI_UINT8_T,
            MPI_UINT8_T,
        };
        MPI_Aint offsets[numitems] = {
            offsetof(struct MPIResponse, src_port),
            offsetof(struct MPIResponse, dst_port),
            offsetof(struct MPIResponse, operation),
            offsetof(struct MPIResponse, status),
        };
        rv = MPI_Type_create_struct(numitems, blocklengths, offsets, types,
                                    &MPI_struct_response);
        errif_return(rv, "Failed to create MPI struct MPIResponse.\n");
        rv = MPI_Type_commit(&MPI_struct_response);
        errif_return(rv, "Failed to commit MPI struct mpi_resp_hader.\n");
    }

    log_verbose("Initialized MPI struct types.\n");
    return 0;
}

/**
 * Initialize the daemon that handles MPI communications.
 */
int MPIServer::InitializeNetwork() {
    const int root = 0;
    int rv;

    log_info("init_mpi_daemon | Initializing ...\n");

    MPI_Barrier(MPI_COMM_WORLD);

    // Rank 0 picks the network ID
    if (rank == root)
        mpi_network_id = rand();

    rv = MPI_Bcast(&mpi_network_id, 1, MPI_INT, root, MPI_COMM_WORLD);
    errif_return(rv, "init_mpi_daemon | Failed to broadcast MPI network ID.\n");
    log_info("init_mpi_daemon | Exchanged MPI network ID %d.\n", mpi_network_id);

    log_info("init_mpi_daemon | Initialized.\n");

    return 0;
}

int MPIServer::InitializeHostnameRankMapping() {
    int rv;
    bool success = true;

    log_verbose("Initializing hostname->rank mapping ...\n");

    string hostname = gethostname();
    const int len = HOST_NAME_MAX;
    char *buf = new char[size * len];
    memset(buf, 0, size * len);
    memcpy(buf + rank * len, hostname.c_str(), hostname.length());

    log_verbose("MPI all gather ...\n");
    rv = MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL,
    //rv = MPI_Allgather(buf + rank * len, len, MPI_CHAR,
                        buf, len, MPI_CHAR, MPI_COMM_WORLD);
    perrif_return(rv, "init hostname mapping | MPI_Allgather");

    log_verbose("Reading hostnames...\n");
    lock_guard lock(hostname_to_rank_mutex);
    for (int r = 0; r < size; r++) {
        string s(buf + r * len);
        log_verbose("hostname = %s, rank = %d.\n", s.c_str(), r);
        auto p = hostname_to_rank.insert(make_pair(s, r));
        if (!p.second) {
            log_error("Duplicate hostname %s.\n", s);
            success = false;
            break;
        }

        string shortname = get_segment(s, ".");
        if (hostname_to_rank.find(shortname) == hostname_to_rank.end())
            hostname_to_rank.insert(make_pair(shortname, r));
    }

    delete[] buf;

    if (success) {
        log_verbose("Initialized hostname->rank mapping.\n");
        return 0;
    } else {
        log_verbose("Failed to initialize hostname->rank mapping.\n");
        return -1;
    }
}

int MPIServer::StartServerThread() {
    ts = thread(&MPIServer::DaemonThread, this);
    return 0;
}

void MPIServer::DaemonThread() {
    log_verbose("mpi_daemon | Started...\n");

    int rv;
    int client_rank;
    short client_port;
    struct MPIRequest request;
    struct MPIResponse response;
    MPI_Status status;
    //bool keep_listening = true;

    do {
        log_info("mpi_daemon | Waiting for request ...\n");

        rv = MPI_Recv(&request, 1, MPI_struct_request, MPI_ANY_SOURCE,
                        MPI_TAG_REQUEST, MPI_COMM_WORLD, &status);
        errif_break(rv, "mpi_daemon | MPI_Recv() failed: %d\n", rv);
        client_rank = status.MPI_SOURCE;
        client_port = request.src_port;
        log_info("mpi_daemon | Received request %s from %d:%hu.\n",
                    to_c_str(request.operation), client_rank, client_port);

        response = {};
        response.src_port = request.dst_port;
        response.dst_port = request.src_port;
        response.operation = request.operation;
        switch (request.operation) {
            case MPIOperation::NOP:
                response.status = MPIStatus::SUCCESS;
                break;
            case MPIOperation::CONNECT: {
                // TODO: implement this
                break;
            }
            default:
                response.status = MPIStatus::PERM;
                log_warning("mpi_daemon | Unhandled operation %s.\n",
                                to_c_str(request.operation));
                break;
        }

        log_info("mpi_daemon | sending response %s to %d:%hu...\n",
                    to_c_str(response.status), client_rank, client_port);
        rv = MPI_Send(&response, 1, MPI_struct_response, client_rank,
                        MPI_TAG_RESPONSE, MPI_COMM_WORLD);
        errif_break(rv, "mpi_daemon | MPI_Send failed: %d\n", rv);

        log_info("mpi_daemon | Processed request from %d:%hu.\n",
                    client_rank, client_port);
    } while (true);

    log_verbose("mpi_daemon | Stopped.\n");
}

MPIServer::MPIServer(int size, int rank) {
    this->size = size;
    this->rank = rank;
}

int MPIServer::Start() {
    int rv = 0;

    if ((rv = InitializeNetwork()) < 0)
        return rv;
    if ((rv = InitializeHostnameRankMapping()) < 0)
        return rv;
    if ((rv = StartServerThread()) < 0)
        return rv;

    return 0;
}

void MPIServer::Stop() {
    // TODO: implement shutdown
}

int MPIServer::GetRank(string hostname) {
    int rank;
    hostname_to_rank_mutex.lock();
    auto it = hostname_to_rank.find(hostname);
    if (it != hostname_to_rank.end())
        rank = it->second;
    else
        rank = -1;
    hostname_to_rank_mutex.unlock();
    return rank;
}

/*
MPIResponse MPIServer::Connect(MPIRequest request) {
    // TODO: implement this
}
*/

#endif // MPI_SERVER_H

