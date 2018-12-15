#include "transport.h"
#include "mpi_transport.h"
#include "bulk_app.h"

extern TransportBase *transport;
extern CommBase *data_comm, *sync_comm;

void init_transport() {
    data_comm = new MPIComm("data comm");
    sync_comm = new MPIComm("sync comm");
    transport = new MPITransport();
}

RequestBase *new_request() {
    return new MPIRequest();
}

RequestBase *new_requests(int count) {
    return new MPIRequest[count];
}

StatusBase *new_status() {
    return new MPIStatus();
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
