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

static void warmup_dummy_connection(int control_host) {
    char warmup_buf[128];
    MPI_Recv(warmup_buf, 128, MPI_CHAR, control_host,
              MPI_ANY_TAG, sync_comm, MPI_STATUS_IGNORE);
}

int run_as_dummy() {
    YAML::Node id_to_rank = load_or_abort(bulk_config, "id_to_rank");
    int control_host = id_to_rank["control"].as<int>();
    uint done_magic = load_or_abort(bulk_config, "done_magic").as<uint>();
    int warmup_iters = load_or_abort(bulk_config, "warmup_iters").as<int>();

    char dummy_buffer[SYNC_PKT_SIZE];

    for(int i = 0; i < warmup_iters; i++)
        warmup_dummy_connection(control_host);

    MPI_Barrier(sync_comm);

    while(true) {
        MPI_Request dummy_req;
        int dummy_recv_done = 0;
        MPI_Irecv(dummy_buffer, SYNC_PKT_SIZE, MPI_CHAR, control_host,
                  MPI_ANY_TAG, sync_comm, &dummy_req);
        while(!dummy_recv_done)
            MPI_Test(&dummy_req, &dummy_recv_done, MPI_STATUS_IGNORE);
        record_stats_entry();

        if (is_magic_pkt(dummy_buffer, SYNC_PKT_SIZE, done_magic)) {
            break;
        }
    }

    return 0;
}
