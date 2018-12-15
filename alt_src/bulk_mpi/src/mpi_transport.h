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
    void Setup(CommBase *data_comm, CommBase *sync_comm) override;
    void Teardown() override;
    void Abort(int code) override;
    //void Warmup(int iterations) override;

    void Barrier(CommBase *comm) override;
    int Isend(char *buf, size_t size, int dst, CommBase *comm, RequestBase *request) override;
    int Irecv(char *buf, size_t size, int src, CommBase *comm, RequestBase *request) override;
    int Send(char *buf, size_t size, int dst, CommBase *comm) override;
    int Recv(char *buf, size_t size, int src, CommBase *comm) override;
    int Waitall(int count, RequestBase *requests) override;
    int Test(RequestBase *request, int *flag, StatusBase *status = NULL) override;
    int Test_cancelled(StatusBase *status, int*flag) override;
    int Cancel(RequestBase *request) override;

    int get_world_rank() override;
    int get_data_rank() override;
    int get_sync_rank() override;

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

void MPITransport::Setup(CommBase *_data_comm, CommBase *_sync_comm) {
    MPIComm *data_comm = dynamic_cast<MPIComm *>(_data_comm);
    MPIComm *sync_comm = dynamic_cast<MPIComm *>(_sync_comm);
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

void MPITransport::Barrier(CommBase *_comm) {
    MPIComm *comm = dynamic_cast<MPIComm *>(_comm);
    MPI_Barrier(*comm->mpi_comm);
}

int MPITransport::Isend(char *buf, size_t size, int dst, CommBase *_comm, RequestBase *_request) {
    MPIComm *comm = dynamic_cast<MPIComm *>(_comm);
    MPIRequest *request = dynamic_cast<MPIRequest *>(_request);
    return MPI_Isend(buf, (int)size, MPI_CHAR, dst, MPI_ANY_TAG, *comm->mpi_comm, &request->mpi_request);
}

int MPITransport::Irecv(char *buf, size_t size, int src, CommBase *_comm, RequestBase *_request) {
    MPIComm *comm = dynamic_cast<MPIComm *>(_comm);
    MPIRequest *request = dynamic_cast<MPIRequest *>(_request);
    return MPI_Irecv(buf, (int)size, MPI_CHAR, src, MPI_ANY_TAG, *comm->mpi_comm, &request->mpi_request);
}

int MPITransport::Send(char *buf, size_t size, int dst, CommBase *_comm) {
    MPIComm *comm = dynamic_cast<MPIComm *>(_comm);
    return MPI_Send(buf, (int)size, MPI_CHAR, dst, MPI_ANY_TAG, *comm->mpi_comm);
}

int MPITransport::Recv(char *buf, size_t size, int src, CommBase *_comm) {
    MPIComm *comm = dynamic_cast<MPIComm *>(_comm);
    return MPI_Recv(buf, (int)size, MPI_CHAR, src, MPI_ANY_TAG, *comm->mpi_comm, MPI_STATUS_IGNORE);
}

int MPITransport::Waitall(int count, RequestBase *_requests) {
    MPIRequest *requests = dynamic_cast<MPIRequest *>(_requests);
    fprintf(stderr, "sizeof(MPIRequest) = %zu, sizeof(MPI_Request) = %zu.\n", sizeof(MPIRequest), sizeof(MPI_Request));
    return MPI_Waitall(count, (MPI_Request *)requests, MPI_STATUSES_IGNORE);
}

int MPITransport::Test(RequestBase *_request, int *flag, StatusBase *_status) {
    MPIRequest *request = dynamic_cast<MPIRequest *>(_request);
    MPIStatus *status = dynamic_cast<MPIStatus *>(_status);
    return MPI_Test(&request->mpi_request, flag, status != NULL ? &status->mpi_status : MPI_STATUS_IGNORE);
}

int MPITransport::Test_cancelled(StatusBase *_status, int*flag) {
    MPIStatus *status = dynamic_cast<MPIStatus *>(_status);
    return MPI_Test_cancelled(&status->mpi_status, flag);
}

int MPITransport::Cancel(RequestBase *_request) {
    MPIRequest *request = dynamic_cast<MPIRequest *>(_request);
    return MPI_Cancel(&request->mpi_request);
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
