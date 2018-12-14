#ifndef MPI_TRANSPORT_H
#define MPI_TRANSPORT_H

#include <mpi.h>
#include "globals.h"
#include "transport.h"

struct MPIRequest;
struct MPIStatus;
class MPIComm;

// MPI Transport
class MPITransport : public TransportBase {
public:
    void Setup(MPIComm *data_comm, MPIComm *sync_comm);
    void Teardown();
    void Abort(int code);
    //void Warmup(int iterations);

    void Barrier(MPIComm *comm);
    int Isend(char *buf, size_t size, int dst, MPIComm *comm, MPIRequest *request);
    int Irecv(char *buf, size_t size, int src, MPIComm *comm, MPIRequest *request);
    int Send(char *buf, size_t size, int dst, MPIComm *comm);
    int Recv(char *buf, size_t size, int src, MPIComm *comm);
    int Waitall(int count, MPIRequest *requests);
    int Test(MPIRequest *request, int *flag, MPIStatus *status = NULL);
    int Test_cancelled(MPIStatus *status, int*flag);
    int Cancel(MPIRequest *request);

    int get_world_rank();
    int get_data_rank();
    int get_sync_rank();

private:
    int world_size, data_world_size, sync_world_size;
    int world_rank, data_rank, sync_rank;
    MPI_Comm data_comm, sync_comm;
};

struct MPIRequest : public RequestBase {
public:
    MPI_Request mpi_request;
};

struct MPIStatus: public StatusBase {
public:
    MPI_Status mpi_status;
};

class MPIComm : public CommBase {
public:
    MPIComm(std::string name);

    MPI_Comm *mpi_comm;
};

void MPITransport::Setup(MPIComm *data_comm, MPIComm *sync_comm) {
    MPI_Init(NULL, NULL);

    // Setup global MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
    // printf("Done with world rank\n");

    // Setup sync MPI
    MPI_Comm_split(MPI_COMM_WORLD, SYNC_COLOR, world_rank, &(this->sync_comm));
    MPI_Comm_size(this->sync_comm, &sync_world_size);
    MPI_Comm_rank(this->sync_comm, &sync_rank);
    // printf("Done with sync rank\n");

    // Setup data MPI
    MPI_Comm_split(MPI_COMM_WORLD, DATA_COLOR, world_rank, &(this->data_comm));
    MPI_Comm_size(this->data_comm, &data_world_size);
    MPI_Comm_rank(this->data_comm, &data_rank);

    data_comm->mpi_comm = &(this->data_comm);
    sync_comm->mpi_comm = &(this->sync_comm);
}

void MPITransport::Teardown() {
    MPI_Finalize();
}

void MPITransport::Abort(int code) {
    MPI_Abort(MPI_COMM_WORLD, code);
}

/*
void MPITransport::Warmup(int iterations) {
    
}
*/

void MPITransport::Barrier(MPIComm *comm) {
    MPI_Barrier(*comm->mpi_comm);
}

int MPITransport::Isend(char *buf, size_t size, int dst, MPIComm *comm, MPIRequest *request) {
    return MPI_Isend(buf, (int)size, MPI_CHAR, dst, MPI_ANY_TAG, *comm->mpi_comm, &request->mpi_request);
}

int MPITransport::Irecv(char *buf, size_t size, int src, MPIComm *comm, MPIRequest *request) {
    return MPI_Irecv(buf, (int)size, MPI_CHAR, src, MPI_ANY_TAG, *comm->mpi_comm, &request->mpi_request);
}

int MPITransport::Send(char *buf, size_t size, int dst, MPIComm *comm) {
    return MPI_Send(buf, (int)size, MPI_CHAR, dst, MPI_ANY_TAG, *comm->mpi_comm);
}

int MPITransport::Recv(char *buf, size_t size, int src, MPIComm *comm) {
    return MPI_Recv(buf, (int)size, MPI_CHAR, src, MPI_ANY_TAG, *comm->mpi_comm, MPI_STATUS_IGNORE);
}

int MPITransport::Waitall(int count, MPIRequest *requests) {
    fprintf(stderr, "sizeof(MPIRequest) = %zu, sizeof(MPI_Request) = %zu.\n", sizeof(MPIRequest), sizeof(MPI_Request));
    MPI_Waitall(count, (MPI_Request *)requests, MPI_STATUSES_IGNORE);
}

int MPITransport::Test(MPIRequest *request, int *flag, MPIStatus *status) {
    return MPI_Test(&request->mpi_request, flag, status != NULL ? &status->mpi_status : MPI_STATUS_IGNORE);
}

int MPITransport::Test_cancelled(MPIStatus *status, int*flag) {
    MPI_Test_cancelled(&status->mpi_status, flag);
}

int MPITransport::Cancel(MPIRequest *request) {
    MPI_Cancel(&request->mpi_request);
}

int MPITransport::get_world_rank() {
    return world_rank;
}

int MPITransport::get_data_rank() {
    return data_rank;
}

int MPITransport::get_sync_rank() {
        return sync_rank;
}

MPIComm::MPIComm(std::string name) : CommBase(name) {
}

#endif  // MPI_TRANSPORT_H
