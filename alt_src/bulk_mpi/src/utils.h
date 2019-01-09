#ifndef BULKMPI_UTILS_H_
#define BULKMPI_UTILS_H_

#include <stdio.h>
#include <string>
#include <cstring>
#include <time.h>
#include "transport.h"
#include <unistd.h>
#include <mpi.h>
#include "yaml-cpp/yaml.h"

extern TransportBase *transport;

#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)     __builtin_expect((x),0)

static inline uint64_t get_time_ns() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (((uint64_t) ts.tv_sec) * 1000000000) + ts.tv_nsec;
}

static inline uint64_t get_us() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (((uint64_t) ts.tv_sec) * 1000000) + (ts.tv_nsec / 1000);
}

static inline uint64_t get_bytes_for_time(uint64_t duration_ns,
                                          uint link_rate_gbps,
                                          uint num_active_rotors) {
    return (duration_ns * link_rate_gbps) / (8 * (num_active_rotors));
}

static inline uint64_t get_time_for_bytes(uint64_t bytes_ns,
                                          uint link_rate_gbps,
                                          uint num_active_rotors) {
    return (bytes_ns * 8 * num_active_rotors) / (link_rate_gbps);
}

static YAML::Node load_or_abort(YAML::Node &config, const char* key) {
    YAML::Node retval = config[key];
    if (retval.Type() == YAML::NodeType::Undefined) {
        fprintf(stderr, "Failed to load '%s' from the config!\n", key);
        transport->Abort(-1);
    }
    return retval;
}

static void wait_for_debugger() {
    int i = 0;
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    printf("PID %d on %s ready for attach\n", getpid(), hostname);
    fflush(stdout);
    while (0 == i)
        sleep(5);
}

static std::string get_short_hostname() {
    const int HOSTNAME_LEN = 256;
    char hostname[HOSTNAME_LEN], hostname_copy[HOSTNAME_LEN];
    if (gethostname(hostname, HOSTNAME_LEN) != 0) {
        perror("gethostname");
        return std::string();
    }

    strcpy(hostname_copy, hostname);
    char *shortname = strtok(hostname_copy, ".");
    if (shortname == NULL)
        shortname = hostname;

    return std::string(shortname);
}

#endif
