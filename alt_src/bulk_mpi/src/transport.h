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
    virtual ~TransportBase();
    virtual void Setup(CommBase *data_comm, CommBase *sync_comm);
    virtual void Teardown();
    virtual void Abort(int code);
    //virtual void Warmup(int iterations);

    virtual void Barrier(CommBase *comm);

    virtual int Isend(char *buf, size_t size, int dst, CommBase *comm, RequestBase *request);
    virtual int Irecv(char *buf, size_t size, int src, CommBase *comm, RequestBase *request);
    virtual int Send(char *buf, size_t size, int dst, CommBase *comm);
    virtual int Recv(char *buf, size_t size, int src, CommBase *comm);
    virtual int Waitall(int count, RequestBase *requests);
    virtual int Test(RequestBase *request, int *flag, StatusBase *status = NULL);
    virtual int Test_cancelled(StatusBase *status, int*flag);
    virtual int Cancel(RequestBase *request);

    virtual int get_world_rank() = 0;
    virtual int get_data_rank() = 0;
    virtual int get_sync_rank() = 0;
};

// Represent a request in the transport
struct RequestBase {
public:
    RequestBase();
    virtual ~RequestBase();
};

// Represent the status of a transport request
struct StatusBase {
public:
    StatusBase();
    virtual ~StatusBase();
};

// Represent a communicator in the transport
class CommBase {
protected:
    std::string name;
public:
    CommBase(std::string name) : name(name) {}
    virtual ~CommBase();
};

#endif  // TRANSPORT_H
