#ifndef PKT_CRAFTER_H_
#define PKT_CRAFTER_H_

#include <arpa/inet.h>
#include "globals.h"

static inline int is_magic_pkt(char* buf, int size, uint magic) {
    if(size < SYNC_PKT_SIZE)
        return 0;

    return ntohl(((uint*)buf)[0]) == magic;
}

static inline int make_pkt(char* buf, int size, uint magic, uint timeslot, uint64_t bytes_ns) {
    if(size < SYNC_PKT_SIZE)
        return 1;

    ((uint*)buf)[0] = htonl(magic);
    ((uint*)buf)[1] = htonl((uint32_t)bytes_ns);
    buf[8] = (char)timeslot;

    return 0;
}

static inline int read_pkt(char* buf, int size, uint magic, uint* timeslot, uint64_t* bytes_ns) {
    if(size < SYNC_PKT_SIZE)
        return 1;

    if(!is_magic_pkt(buf, size, magic))
        return 1;

    *bytes_ns = (uint64_t) ntohl(((uint*)buf)[1]);
    *timeslot = (uint)(buf[8]);

    return 0;
}

#endif
