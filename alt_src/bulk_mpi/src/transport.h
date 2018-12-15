#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <cstdlib>
#include <string>

struct RequestBase;
struct StatusBase;
class CommBase;

// Generic transport layer
class TransportBase {
public:
    virtual ~TransportBase() {}
    virtual void Setup(CommBase *data_comm, CommBase *sync_comm) = 0;
    virtual void Teardown() = 0;
    virtual void Abort(int code) = 0;
    //virtual void Warmup(int iterations) = 0;

    virtual void Barrier(CommBase *comm) = 0;

    virtual int Isend(char *buf, size_t size, int dst, CommBase *comm, RequestBase *request) = 0;
    virtual int Irecv(char *buf, size_t size, int src, CommBase *comm, RequestBase *request) = 0;
    virtual int Send(char *buf, size_t size, int dst, CommBase *comm) = 0;
    virtual int Recv(char *buf, size_t size, int src, CommBase *comm) = 0;
    virtual int Waitall(int count, RequestBase *requests) = 0;
    virtual int Test(RequestBase *request, int *flag, StatusBase *status = NULL) = 0;
    virtual int Test_cancelled(StatusBase *status, int*flag) = 0;
    virtual int Cancel(RequestBase *request) = 0;

    virtual int get_world_rank() = 0;
    virtual int get_data_rank() = 0;
    virtual int get_sync_rank() = 0;
};

// Represent a request in the transport
struct RequestBase {
public:
    RequestBase() {}
    virtual ~RequestBase() {}
};

// Represent the status of a transport request
struct StatusBase {
public:
    StatusBase() {}
    virtual ~StatusBase() {}
};

// Represent a communicator in the transport
class CommBase {
protected:
    std::string name;
public:
    CommBase(std::string name) : name(name) {}
    virtual ~CommBase() {}
};

#endif  // TRANSPORT_H
