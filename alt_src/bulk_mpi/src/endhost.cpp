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
#include "sha256.h"

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
static std::map<int, struct fake_endhost_data*> inc_data_map, out_data_map;

static uint64_t timeslot_received, ts_allocation, ts_end, ts_fill, ns_per_send;
static MPI_Request ts_request, inc_request, out_request;
static int out_bytes, inc_bytes;
static int tsq_in_progress, inc_in_progress, out_in_progress;
static char ts_buffer[SYNC_PKT_SIZE];

static uint64_t started_run, ended_run;

struct delta_stats {
    uint64_t entries[NUM_STAT_ENTRIES];
    int cnt;
};
static struct delta_stats* bulk_stats;
static uint64_t bulk_started, bulk_done;
static uint out_cancelled, inc_cancelled;

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
        if(rotor_state == 0 || affected_rotor != my_rotor)
            continue;

        int target_id = mappings[affected_rotor][rotor_state][my_id].as<int>();
        int target_rank = id_to_rank[target_id][my_rotor].as<int>();

        if(target_rank == my_rank)
            continue;

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
        inc_data_map[target_rank] = target_inc;
        out_data_map[target_rank] = target_out;
    }

    num_targets = ts_to_target.size();
    targets_done = 0;

    return 0;

setup_mappings_fail:
    for(int i = 0; i < num_ts; i++) {
        if(ts_to_target.count(i)){
            int target_rank = ts_to_target[i];
            if(inc_data_map.count(target_rank))
                free_endhost_data(inc_data_map[target_rank]);
            if(out_data_map.count(target_rank))
                free_endhost_data(out_data_map[target_rank]);
        }
    }
    return -1;
}

static void free_mappings() {
    for(auto it = inc_data_map.begin(); it != inc_data_map.end(); it++) {
        int target = it->first;
        struct fake_endhost_data* target_inc = inc_data_map[target];
        struct fake_endhost_data* target_out = out_data_map[target];

        free_endhost_data(target_inc);
        free_endhost_data(target_out);
    }
}

static void warmup_connections() {
    MPI_Request warmup_sends[num_targets];
    MPI_Request warmup_recvs[num_targets];
    char random_send[64];
    char random_recv[num_targets][64];

    int i = 0;
    for(auto it = ts_to_target.begin(); it != ts_to_target.end(); it++){
        MPI_Isend(&random_send[0], 64, MPI_CHAR, it->second,
                  0, data_comm, &warmup_sends[i]);
        MPI_Irecv(&random_recv[i][0], 64, MPI_CHAR, it->second,
                  MPI_ANY_TAG, data_comm, &warmup_recvs[i]);
        i++;
    }

    // printf("#%d: Waiting on %d bidir requests to ", my_rank, num_targets);
    // for(auto it = ts_to_target.begin(); it != ts_to_target.end(); it++){
    //     printf("%d, ", it->second);
    // }
    // printf("\n");
    MPI_Waitall(num_targets, &warmup_sends[0], MPI_STATUSES_IGNORE);
    // printf("#%d: Sends done.\n", my_rank);
    MPI_Waitall(num_targets, &warmup_recvs[0], MPI_STATUSES_IGNORE);
    // printf("#%d: Recvs done.\n", my_rank);
}

static int update_timeslot() {
    if(is_magic_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic)){
        if(read_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic,
                    (uint*)&current_ts_id, &ts_allocation))
            return 1;

        current_ts = timeslots[current_ts_id];
        if(!ts_to_target.count(current_ts_id)
        || current_ts["affected_rotor"].as<uint>() != my_rotor
        || current_ts["rotor_state"].as<int>() == 0)
            return 1;

        current_target = ts_to_target[current_ts_id];

        // ts_end = timeslot_received + (current_ts["byte_allocation_us"].as<uint64_t>() * 1000);
        // ts_fill = get_time_ns();
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
        // timeslot_received = get_time_ns();
        tsq_in_progress = 0;
        return 1;
    }
    else
        return 0;
}

static void start_new_transfers() {
    char *out_buffer, *inc_buffer;
    struct fake_endhost_data* inc_edata;
    struct fake_endhost_data* out_edata;

    if(!inc_in_progress) {
        inc_edata = inc_data_map[current_target];
        if(recv_next_endhost_data(inc_edata, &inc_buffer, &inc_bytes, warmup_iters) == 0){
            inc_bytes = std::min((uint64_t)inc_bytes, ts_allocation);
            MPI_Irecv(inc_buffer, inc_bytes, MPI_CHAR, current_target,
                      MPI_ANY_TAG, data_comm, &inc_request);
            inc_in_progress = 1;
        }
    }

    if(!out_in_progress) {
        out_edata = out_data_map[current_target];
        if(send_next_endhost_data(out_edata, &out_buffer, &out_bytes, warmup_iters) == 0){
            out_bytes = std::min((uint64_t)out_bytes, ts_allocation);
            bulk_started = get_time_ns();
            MPI_Isend(out_buffer, out_bytes, MPI_CHAR, current_target,
                      0, data_comm, &out_request);
            out_in_progress = 1;
        }
    }
}

static void check_transfer_states() {
    if(inc_in_progress) {
        int inc_done = 0;
        MPI_Test(&inc_request, &inc_done, MPI_STATUS_IGNORE);
        if(inc_done) {
            recv_done_endhost_data(inc_data_map[current_target], inc_bytes, warmup_iters);
            inc_in_progress = 0;
        }
    }

    if(out_in_progress) {
        int out_done = 0;
        MPI_Test(&out_request, &out_done, MPI_STATUS_IGNORE);
        if(out_done){
            bulk_done = get_time_ns();
            bulk_stats->entries[bulk_stats->cnt++] = bulk_done - bulk_started;
            send_done_endhost_data(out_data_map[current_target], out_bytes, warmup_iters);
            out_in_progress = 0;
        }
    }

}

// Don't use this. MPI_Request_free is _not_ safe and will cause segfaults.
// static void terminate_transfers() {
//     if(inc_in_progress) {
//         MPI_Cancel(&inc_request);
//         MPI_Request_free(&inc_request);
//         inc_in_progress = 0;
//     }
//     if(out_in_progress) {
//         MPI_Cancel(&out_request);
//         MPI_Request_free(&out_request);
//         out_in_progress = 0;
//     }
// }

static void terminate_transfers_and_wait() {
    if(inc_in_progress) {
        MPI_Cancel(&inc_request);
        int inc_done = 0;
        while(!inc_done)
            MPI_Test(&inc_request, &inc_done, MPI_STATUS_IGNORE);
        inc_in_progress = 0;
    }
    if(out_in_progress) {
        MPI_Cancel(&out_request);
        int out_done = 0;
        while(!out_done)
            MPI_Test(&out_request, &out_done, MPI_STATUS_IGNORE);
        out_in_progress = 0;
    }
}

static void write_endhost_results() {
    char fname[128];
    snprintf(fname, 128, "/tmp/endhost_results_rank_%d.txt", my_rank);
    FILE *resf = fopen(fname, "w");
    if(resf == NULL){
        fprintf(stderr, "Failed to write endhost results for rank %d\n", my_rank);
        return;
    }

    for(auto it = inc_data_map.begin(); it != inc_data_map.end(); it++) {
        fprintf(resf, "RECV %d->%d:", my_rank, it->first);
        struct fake_endhost_data *edata = it->second;
        for(int i = 0; i < edata->num_bufs; i++) {
            fprintf(resf, " %s", sha256(std::string(edata->data_arrs[i], edata->buf_size)).c_str());
        }
        fprintf(resf, "\n");
    }

    for(auto it = out_data_map.begin(); it != out_data_map.end(); it++) {
        fprintf(resf, "SEND %d->%d:", my_rank, it->first);
        struct fake_endhost_data *edata = it->second;
        for(int i = 0; i < edata->num_bufs; i++) {
            fprintf(resf, " %s", sha256(std::string(edata->data_arrs[i], edata->buf_size)).c_str());
        }
        fprintf(resf, "\n");
    }

    fclose(resf);
}

static void write_endhost_stats() {
    char fname[128];
    snprintf(fname, 128, "/tmp/endhost_stats_rank_%d.txt", my_rank);
    FILE *statsf = fopen(fname, "w");
    if(statsf == NULL){
        fprintf(stderr, "Failed to write endhost stats for rank %d\n", my_rank);
        return;
    }

    for(int i = 2; i < bulk_stats->cnt; i++) {
        fprintf(statsf, "%lu ", bulk_stats->entries[i]);
    }
    fprintf(statsf, "\n");
    fclose(statsf);
}

static void setup_endhost_stats() {
    bulk_stats = new delta_stats;
    bulk_stats->cnt = 0;
    out_cancelled = 0;
    inc_cancelled = 0;
}

static void free_endhost_stats() {
    delete bulk_stats;
}

int run_bulk_endhost() {
    setup_from_yaml();
    if(setup_mappings() != 0){
        fprintf(stderr, "Host %d: Failed to setup mappings\n", my_rank);
        return -1;
    }

    setup_endhost_stats();

    warmup_connections();

    MPI_Barrier(sync_comm);

    started_run = get_time_ns();
    start_timeslot_recv();
    while (targets_done != num_targets) {
        if(check_for_timeslot()) {
            switch(update_timeslot()) {
                case 0:
                    record_stats_entry();
                    terminate_transfers_and_wait();
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

    write_endhost_results();
    write_endhost_stats();

    terminate_transfers_and_wait();

    free_mappings();
    free_endhost_stats();

    return 0;
}

int run_as_endhost(uint rank_val) {
    bool send_bulk_data = load_or_abort(bulk_config, "send_bulk_data").as<bool>();
    my_rank = rank_val;

    if(send_bulk_data)
        return run_bulk_endhost();
    else
        return run_as_dummy();
}
