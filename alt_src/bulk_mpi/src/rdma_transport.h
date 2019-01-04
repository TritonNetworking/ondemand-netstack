#ifndef RDMA_TRANSPORT_H
#define RDMA_TRANSPORT_H

#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

#include "globals.h"
#include "transport.h"

struct RDMARequest;
struct RDMAStatus;
class RDMAComm;

// RDMA Transport
class RDMATransport : public TransportBase {
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
    // TODO
};

struct RDMARequest : public RequestBase {
public:
};

struct RDMAStatus: public StatusBase {
public:
};

class RDMAComm : public CommBase {
public:
    RDMAComm(std::string name);

    // TODO: QP info?
};

void RDMATransport::Setup(CommBase *_data_comm, CommBase *_sync_comm) {
    RDMAComm *data_comm = dynamic_cast<RDMAComm *>(_data_comm);
    RDMAComm *sync_comm = dynamic_cast<RDMAComm *>(_sync_comm);

    // TODO
}

void RDMATransport::Teardown() {
    // TODO
}

void RDMATransport::Abort(int code) {
    // TODO
}

/*
void RDMATransport::Warmup(int iterations) {
    
}
*/

void RDMATransport::Barrier(CommBase *_comm) {
    RDMAComm *comm = dynamic_cast<RDMAComm *>(_comm);
    // TODO
}

int RDMATransport::Isend(char *buf, size_t size, int dst, CommBase *_comm, RequestBase *_request) {
    RDMAComm *comm = dynamic_cast<RDMAComm *>(_comm);
    // TODO
}

int RDMATransport::Irecv(char *buf, size_t size, int src, CommBase *_comm, RequestBase *_request) {
    RDMAComm *comm = dynamic_cast<RDMAComm *>(_comm);
    // TODO
}

int RDMATransport::Send(char *buf, size_t size, int dst, CommBase *_comm) {
    RDMAComm *comm = dynamic_cast<RDMAComm *>(_comm);
    // TODO
}

int RDMATransport::Recv(char *buf, size_t size, int src, CommBase *_comm) {
    RDMAComm *comm = dynamic_cast<RDMAComm *>(_comm);
    // TODO
}

int RDMATransport::Waitall(int count, RequestBase *_requests) {
    RDMARequest *requests = dynamic_cast<RDMARequest *>(_requests);
    // TODO
}

int RDMATransport::Test(RequestBase *_request, int *flag, StatusBase *_status) {
    RDMARequest *request = dynamic_cast<RDMARequest *>(_request);
    RDMAStatus *status = dynamic_cast<RDMAStatus *>(_status);
    // TODO
}

int RDMATransport::Test_cancelled(StatusBase *_status, int*flag) {
    RDMAStatus *status = dynamic_cast<RDMAStatus *>(_status);
    // TODO
}

int RDMATransport::Cancel(RequestBase *_request) {
    RDMARequest *request = dynamic_cast<RDMARequest *>(_request);
    // TODO
}

int RDMATransport::get_world_rank() {
    // TODO
}

int RDMATransport::get_data_rank() {
    // TODO
}

int RDMATransport::get_sync_rank() {
    // TODO
}

RDMAComm::RDMAComm(std::string name) : CommBase(name) {
    // TODO
}

#endif  // RDMA_TRANSPORT_H
