#ifndef BULKMPI_UTILS_H_
#define BULKMPI_UTILS_H_

#include <stdio.h>
#include <time.h>
#include <mpi.h>
#include "yaml-cpp/yaml.h"

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

static YAML::Node load_or_abort(YAML::Node &config, const char* key) {
    YAML::Node retval = config[key];
    if (retval.Type() == YAML::NodeType::Undefined) {
        fprintf(stderr, "Failed to load '%s' from the config!\n", key);
        MPI_Abort(MPI_COMM_WORLD, -1);
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

#endif
