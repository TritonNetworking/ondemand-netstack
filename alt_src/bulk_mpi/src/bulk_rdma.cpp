#include "transport.h"
#include "rdma_transport.h"
#include "bulk_app.h"

extern TransportBase *transport;
extern CommBase *data_comm, *sync_comm;

void init_transport() {
    data_comm = new RDMAComm("data comm");
    sync_comm = new RDMAComm("sync comm");
    transport = new RDMATransport();
}

RequestBase *new_request() {
    return new RDMARequest();
}

RequestBase *new_requests(int count) {
    return new RDMARequest[count];
}

StatusBase *new_status() {
    return new RDMAStatus();
}

void free_request(RequestBase *request) {
    delete request;
}

void free_requests(RequestBase *requests) {
    delete[] requests;
}

void free_status(StatusBase *status) {
    delete status;
}
