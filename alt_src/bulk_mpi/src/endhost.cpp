#include <cstdlib>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include "yaml-cpp/yaml.h"
#include "globals.h"
#include "utils.h"
#include "pkt_crafter.h"
#include "run_funcs.h"
#include "stats.h"

int run_as_endhost(uint id_val) {
    YAML::Node id_to_rank = load_or_abort(bulk_config, "id_to_rank");
    int control_host = id_to_rank["control"].as<int>();
    uint done_magic = load_or_abort(bulk_config, "done_magic").as<uint>();

    char sync_buffer[SYNC_PKT_SIZE];
    while(true) {
        MPI_Request sync_req;
        int sync_recv_done = 0;
        MPI_Irecv(sync_buffer, SYNC_PKT_SIZE, MPI_CHAR, control_host,
                  MPI_ANY_TAG, sync_comm, &sync_req);
        while(!sync_recv_done)
            MPI_Test(&sync_req, &sync_recv_done, MPI_STATUS_IGNORE);
        record_stats_entry();

        if (is_magic_pkt(sync_buffer, SYNC_PKT_SIZE, done_magic)) {
            break;
        }
    }

    return 0;
}