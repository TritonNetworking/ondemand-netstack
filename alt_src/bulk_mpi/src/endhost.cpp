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
#include "fake_data.h"

static uint64_t now;
static uint my_rank, my_id, my_rotor;
static int control_host;
static uint link_rate_gbps, bytes_per_req;
static uint endhost_magic, done_magic;
static int num_hosts, num_states, num_rotors;

static int num_ts, current_ts_id, current_target;
static YAML::Node current_ts;
static YAML::Node timeslots, mappings;

static YAML::Node id_to_rank, rank_to_id, rank_to_rotor;

static int chunk_size_bytes, num_chunks, warmup_iters;
static int targets_done, num_targets;
static std::map<int, int> ts_to_target;
static std::map<int, struct fake_endhost_data*> inc_data, out_data;

static uint64_t timeslot_received, ts_allocation, ts_end, ts_fill, ns_per_send;
static MPI_Request ts_request, inc_request, out_request;
static int tsq_in_progress, inc_in_progress, out_in_progress;
static char ts_buffer[SYNC_PKT_SIZE];

static uint64_t started_run, ended_run;

static void setup_from_yaml() {
    link_rate_gbps = load_or_abort(bulk_config, "link_rate_gbps").as<uint>();
    bytes_per_req = load_or_abort(bulk_config, "bytes_per_req").as<uint>();
    chunk_size_bytes = load_or_abort(bulk_config, "chunk_size_mb").as<int>() * (1<<20);
    num_chunks = load_or_abort(bulk_config, "num_chunks").as<int>();
    warmup_iters = load_or_abort(bulk_config, "warmup_iters").as<int>();

    num_hosts = load_or_abort(bulk_config, "num_hosts").as<int>();
    num_states = load_or_abort(bulk_config, "num_states").as<int>();
    num_rotors = load_or_abort(bulk_config, "num_rotors").as<int>();

    id_to_rank = load_or_abort(bulk_config, "id_to_rank");
    rank_to_id = load_or_abort(bulk_config, "rank_to_id");
    rank_to_rotor = load_or_abort(bulk_config, "rank_to_rotor");

    endhost_magic = load_or_abort(bulk_config, "endhost_magic").as<uint>();
    done_magic = load_or_abort(bulk_config, "done_magic").as<uint>();

    num_ts = (int)load_or_abort(bulk_config, "timeslot_order").as<std::vector<uint>>().size();
    current_ts_id = -1;
    timeslots = load_or_abort(bulk_config, "timeslots");
    mappings = load_or_abort(bulk_config, "mappings");

    my_id = rank_to_id[my_rank].as<int>();
    my_rotor = rank_to_rotor[my_rank].as<int>();
    control_host = id_to_rank["control"].as<int>();
    ns_per_send = get_time_for_bytes(bytes_per_req, link_rate_gbps, num_rotors - 1);
}

static int setup_mappings() {
    for(int i = 0; i < num_ts; i++) {
        int affected_rotor = timeslots[i]["affected_rotor"].as<int>();
        int rotor_state = timeslots[i]["rotor_state"].as<int>();
        if(rotor_state == 0)
            continue;

        int target_id = mappings[affected_rotor][rotor_state][my_id].as<int>();
        int target_rank = id_to_rank[target_id][my_rotor].as<int>();

        struct fake_endhost_data* target_inc = allocate_fake_data(target_rank,
                                                                  my_rank,
                                                                  chunk_size_bytes,
                                                                  num_chunks);
        struct fake_endhost_data* target_out = allocate_fake_data(target_rank,
                                                                  my_rank,
                                                                  chunk_size_bytes,
                                                                  num_chunks);
        if(target_inc == NULL || target_out == NULL || fill_endhost_data(target_out) != 0) {
            free_endhost_data(target_inc);
            free_endhost_data(target_out);
            goto setup_mappings_fail;
        }

        ts_to_target[i] = target_rank;
        inc_data[target_rank] = target_inc;
        out_data[target_rank] = target_out;
    }

    num_targets = ts_to_target.size();
    targets_done = 0;

    return 0;

setup_mappings_fail:
    for(int i = 0; i < num_ts; i++) {
        if(ts_to_target.count(i)){
            int target_rank = ts_to_target[i];
            if(inc_data.count(target_rank))
                free_endhost_data(inc_data[target_rank]);
            if(out_data.count(target_rank))
                free_endhost_data(out_data[target_rank]);
        }
    }
    return -1;
}

static void free_mappings() {
    for(auto it = inc_data.begin(); it != inc_data.end(); it++) {
        int target = it->first;
        struct fake_endhost_data* target_inc = inc_data[target];
        struct fake_endhost_data* target_out = out_data[target];

        free_endhost_data(target_inc);
        free_endhost_data(target_out);
    }
}

static int update_timeslot() {
    if(is_magic_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic)){
        if(read_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic,
                    (uint*)&current_ts_id, &ts_allocation))
            return 1;

        current_ts = timeslots[current_ts_id];
        if(current_ts["affected_rotor"].as<uint>() != my_rotor
        || current_ts["rotor_state"].as<int>() == 0)
            return 1;

        current_target = ts_to_target[current_ts_id];

        ts_end = timeslot_received + (current_ts["byte_allocation_us"].as<uint64_t>() * 1000);
        ts_fill = get_time_ns();
        return 0;
    } else if(is_magic_pkt(ts_buffer, SYNC_PKT_SIZE, done_magic))
        return 2;
    else
        return -1;
}

static void start_timeslot_recv() {
    if(tsq_in_progress)
        return;

    MPI_Irecv(ts_buffer, SYNC_PKT_SIZE, MPI_CHAR, control_host,
              MPI_ANY_TAG, sync_comm, &ts_request);
    tsq_in_progress = 1;
}

static int check_for_timeslot() {
    if(!tsq_in_progress)
        start_timeslot_recv();

    int tsq_done = 0;
    MPI_Test(&ts_request, &tsq_done, MPI_STATUS_IGNORE);
    if (tsq_done){
        timeslot_received = get_time_ns();
        tsq_in_progress = 0;
        return 1;
    }
    else
        return 0;
}

static void terminate_transfers() {

}

static void start_new_transfers() {

}

static void check_transfer_states() {

}

static void write_endhost_results() {

}

int run_bulk_endhost() {
    setup_from_yaml();
    if(setup_mappings() != 0){
        fprintf(stderr, "Host %d: Failed to setup mappings\n", my_rank);
        return -1;
    }

    started_run = get_time_ns();
    start_timeslot_recv();
    while (targets_done != num_targets) {
        if(check_for_timeslot()) {
            switch(update_timeslot()) {
                case 0:
                    record_stats_entry();
                    terminate_transfers();
                    start_new_transfers();
                    start_timeslot_recv();
                    break;
                case 1:
                    break;
                case 2:
                    goto endhost_done;
                default:
                    break;
            }
        }
        check_transfer_states();
    }

endhost_done:
    ended_run = get_time_ns();
    terminate_transfers();
    write_endhost_results();

    free_mappings();
    return 0;
}

int run_as_endhost(uint rank_val) {
    int send_bulk_data = load_or_abort(bulk_config, "send_bulk_data").as<int>();
    my_rank = rank_val;

    if(send_bulk_data)
        return run_bulk_endhost();
    else
        return run_as_dummy();
}
