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

#include "bulk_app.h"

static uint64_t now;
static uint my_rank, my_id, my_rotor;
static int control_host;
static uint link_rate_gbps, bytes_per_req;
static uint endhost_magic, done_magic;
static int num_hosts, num_states, num_rotors;
static int warmup_iters;

static int num_ts, current_ts_id, current_target;
static std::map<std::string, int> current_ts;
static std::map<int, std::map<std::string, int>> timeslots;
static std::map<int, std::map<int, std::vector<int>>> mappings;
static std::vector<uint> timeslot_order;

static YAML::Node id_to_rank, rank_to_id, rank_to_rotor;

static int chunk_size_bytes, num_chunks;
static int targets_done, num_targets;
static std::map<int, int> ts_to_target;
static std::map<int, bool> target_recv_done, target_send_done;
static std::map<int, struct fake_endhost_data*> inc_data_map, out_data_map;

static uint64_t timeslot_received, ts_allocation, ts_end, ts_fill, ns_per_send;
// static uint64_t ts_send_allocation, ts_recv_allocation;
static RequestBase *ts_request, *inc_request, *out_request;
static int out_target, inc_target;
static int out_bytes, inc_bytes;
static int tsq_in_progress, inc_in_progress, out_in_progress;
static char ts_buffer[SYNC_PKT_SIZE];

static uint64_t started_run, ended_run;

extern CommBase *sync_comm, *data_comm;

struct delta_stats {
    uint64_t entries[NUM_STAT_ENTRIES];
    int cnt;
};
static struct delta_stats *bulk_stats, *flow_stats;
static uint64_t bulk_started, bulk_done, flow_started, flow_done;
static int out_missed, inc_missed;

static void setup_from_yaml() {
    link_rate_gbps = load_or_abort(bulk_config, "link_rate_gbps").as<uint>();
    bytes_per_req = load_or_abort(bulk_config, "bytes_per_req").as<uint>();
    chunk_size_bytes = load_or_abort(bulk_config, "chunk_size_kb").as<int>() * (1<<10);
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

    timeslot_order = load_or_abort(bulk_config, "timeslot_order").as<std::vector<uint>>();
    num_ts = (int)timeslot_order.size();
    current_ts_id = -1;
    timeslots = load_or_abort(bulk_config, "timeslots").as<std::map<int, std::map<std::string, int>>>();
    mappings = load_or_abort(bulk_config, "mappings").as<std::map<int, std::map<int, std::vector<int>>>>();

    my_id = rank_to_id[my_rank].as<int>();
    my_rotor = rank_to_rotor[my_rank].as<int>();
    control_host = id_to_rank["control"].as<int>();
    ns_per_send = get_time_for_bytes(bytes_per_req, link_rate_gbps, num_rotors - 1);
}

static int setup_mappings() {
    for(int i = 0; i < num_ts; i++) {
        int next_ts_id = timeslot_order[i];
        int affected_rotor = timeslots[next_ts_id]["affected_rotor"];
        int rotor_state = timeslots[next_ts_id]["rotor_state"];
        if(rotor_state == 0 || affected_rotor != my_rotor)
            continue;

        int target_id = mappings[affected_rotor][rotor_state][my_id];
        int target_rank = id_to_rank[target_id][my_rotor].as<int>();

        if(target_rank == my_rank)
            continue;

        struct fake_endhost_data* target_inc = allocate_fake_data(target_rank,
                                                                  my_rank,
                                                                  chunk_size_bytes,
                                                                  num_chunks);
        struct fake_endhost_data* target_out = allocate_fake_data(my_rank,
                                                                  target_rank,
                                                                  chunk_size_bytes,
                                                                  num_chunks);
        if(target_inc == NULL || target_out == NULL || fill_endhost_data(target_out) != 0) {
            free_endhost_data(target_inc);
            free_endhost_data(target_out);
            goto setup_mappings_fail;
        }

        ts_to_target[next_ts_id] = target_rank;
        target_recv_done[target_rank] = false;
        target_send_done[target_rank] = false;
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

static void warmup_sync_connections() {
    char fakebuf[64];
    transport->Recv(fakebuf, 64, control_host, sync_comm);
}

static void warmup_data_connections() {
    auto *warmup_sends = new_requests(num_targets);
    auto *warmup_recvs = new_requests(num_targets);
    char random_send[1024];
    char random_recv[num_targets][1024];

    int i = 0;
    for(auto it = ts_to_target.begin(); it != ts_to_target.end(); it++){
        transport->Isend(&random_send[0], 1024, it->second,
                  data_comm, &warmup_sends[i]);
        transport->Irecv(&random_recv[i][0], 1024, it->second,
                  data_comm, &warmup_recvs[i]);
        i++;
    }

    // printf("#%d: Waiting on %d bidir requests to ", my_rank, num_targets);
    // for(auto it = ts_to_target.begin(); it != ts_to_target.end(); it++){
    //     printf("%d, ", it->second);
    // }
    // printf("\n");
    transport->Waitall(num_targets, warmup_sends);
    // printf("#%d: Sends done.\n", my_rank);
    transport->Waitall(num_targets, warmup_recvs);
    // printf("#%d: Recvs done.\n", my_rank);
}

static int update_timeslot() {
    if(is_magic_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic)){
        if(read_pkt(ts_buffer, SYNC_PKT_SIZE, endhost_magic,
                    (uint*)&current_ts_id, &ts_allocation)){
            fprintf(stderr, "#%d: got packet from control host that wasn't a control packet\n", my_rank);
            return 1;
        }

        current_ts = timeslots[current_ts_id];
        if(current_ts["affected_rotor"] != my_rotor
        || current_ts["rotor_state"] == 0) {
            fprintf(stderr, "#%d: got erroneous timeslot ID %d, affected_rotor=%u, rotor_state=%d\n",
                    my_rank, current_ts_id,
                    current_ts["affected_rotor"],
                    current_ts["rotor_state"]);
            return 1;
        }
        if(!ts_to_target.count(current_ts_id))
            return 1;

        current_target = ts_to_target[current_ts_id];

        // ts_send_allocation = ts_allocation;
        // ts_recv_allocation = ts_allocation;

        // ts_end = timeslot_received + (current_ts["byte_allocation_us"].as<uint64_t>() * 1000);
        // ts_fill = get_time_ns();

        // printf("#%d: timeslot received. id=%d, affected_rotor=%d, rotor_state=%d, allocation=%lu, target=%d\n",
        //        my_rank, current_ts_id, current_ts["affected_rotor"], current_ts["rotor_state"], ts_allocation, current_target);

        return 0;
    } else if(is_magic_pkt(ts_buffer, SYNC_PKT_SIZE, done_magic))
        return 2;
    else
        return -1;
}

static void start_timeslot_recv() {
    if(tsq_in_progress)
        return;

    transport->Irecv(ts_buffer, SYNC_PKT_SIZE, control_host,
              sync_comm, ts_request);
    tsq_in_progress = 1;
}

static int check_for_timeslot() {
    if(!tsq_in_progress)
        start_timeslot_recv();

    int tsq_done = 0;
    transport->Test(ts_request, &tsq_done);
    if (tsq_done){
        // timeslot_received = get_time_ns();
        tsq_in_progress = 0;
        return 1;
    }
    else
        return 0;
}

static void start_new_transfers(int new_timeslot) {
    char *out_buffer, *inc_buffer;
    struct fake_endhost_data* inc_edata;
    struct fake_endhost_data* out_edata;

    if(inc_in_progress && !target_recv_done[current_target] && new_timeslot)
        inc_missed++;
    if(!inc_in_progress && !target_recv_done[current_target]) {
        inc_edata = inc_data_map[current_target];
        if(recv_next_endhost_data(inc_edata, &inc_buffer, &inc_bytes) == 0){
            inc_bytes = std::min((uint64_t)inc_bytes, ts_allocation);
            transport->Irecv(inc_buffer, inc_bytes, current_target,
                      data_comm, inc_request);
            inc_in_progress = 1;
            inc_target = current_target;
        }
    }

    if(out_in_progress && !target_send_done[current_target] && new_timeslot)
        out_missed++;
    if(!out_in_progress && !target_send_done[current_target]) {
        out_edata = out_data_map[current_target];
        if(send_next_endhost_data(out_edata, &out_buffer, &out_bytes) == 0){
            out_bytes = std::min((uint64_t)out_bytes, ts_allocation);
            bulk_started = get_time_ns();
            transport->Isend(out_buffer, out_bytes, current_target,
                      data_comm, out_request);
            out_in_progress = 1;
            out_target = current_target;
        }
    }
}

static int check_transfer_states() {
    int ret = 0;

    if(inc_in_progress) {
        int inc_done = 0;
        transport->Test(inc_request, &inc_done);
        if(inc_done) {
            if(recv_done_endhost_data(inc_data_map[inc_target], inc_bytes) == 1){
                target_recv_done[inc_target] = true;
                if(target_send_done[inc_target] && target_recv_done[inc_target]){
                    flow_done = get_time_ns();
                    flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                    flow_started = get_time_ns();
                    targets_done++;
                }
            }
            inc_in_progress = 0;
            ret = 1;
        }
    }

    if(out_in_progress) {
        int out_done = 0;
        transport->Test(out_request, &out_done);
        if(out_done){
            bulk_done = get_time_ns();
            bulk_stats->entries[bulk_stats->cnt++] = bulk_done - bulk_started;
            if(send_done_endhost_data(out_data_map[out_target], out_bytes) == 1){
                target_send_done[out_target] = true;
                if(target_send_done[out_target] && target_recv_done[out_target]){
                    flow_done = get_time_ns();
                    flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                    flow_started = get_time_ns();
                    targets_done++;
                }
            }
            out_in_progress = 0;
            ret = 1;
        }
    }

    return ret;
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
        transport->Cancel(inc_request);

        int inc_done = 0;
        auto *inc_status = new_status();
        while(!inc_done)
            transport->Test(inc_request, &inc_done, inc_status);

        int inc_cancelled = 0;
        transport->Test_cancelled(inc_status, &inc_cancelled);
        free_status(inc_status);

        if(!inc_cancelled){
            if(recv_done_endhost_data(inc_data_map[inc_target], inc_bytes) == 1){
                target_recv_done[inc_target] = true;
                if(target_send_done[inc_target] && target_recv_done[inc_target]){
                    flow_done = get_time_ns();
                    flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                    flow_started = get_time_ns();
                    targets_done++;
                }
            }
        }
        inc_in_progress = 0;
    }
    if(out_in_progress) {
        transport->Cancel(out_request);

        int out_done = 0;
        auto *out_status = new_status();
        while(!out_done)
            transport->Test(out_request, &out_done, out_status);

        int out_cancelled = 0;
        transport->Test_cancelled(out_status, &out_cancelled);
        free_status(out_status);

        if(!out_cancelled){
            bulk_done = get_time_ns();
            bulk_stats->entries[bulk_stats->cnt++] = bulk_done - bulk_started;
            if(send_done_endhost_data(out_data_map[out_target], out_bytes) == 1){
                target_send_done[out_target] = true;
                if(target_send_done[out_target] && target_recv_done[out_target]){
                    flow_done = get_time_ns();
                    flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                    flow_started = get_time_ns();
                    targets_done++;
                }
            }
        }
        out_in_progress = 0;
    }
}

static void wait_for_transfers() {
    if(inc_in_progress) {
        int inc_done = 0;
        while(!inc_done)
            transport->Test(inc_request, &inc_done);

        if(recv_done_endhost_data(inc_data_map[inc_target], inc_bytes) == 1){
            target_recv_done[inc_target] = true;
            if(target_send_done[inc_target] && target_recv_done[inc_target]){
                flow_done = get_time_ns();
                flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                flow_started = get_time_ns();
                targets_done++;
            }
        }
        inc_in_progress = 0;
    }

    if(out_in_progress) {
        int out_done = 0;
        while(!out_done)
            transport->Test(out_request, &out_done);
        bulk_done = get_time_ns();
        bulk_stats->entries[bulk_stats->cnt++] = bulk_done - bulk_started;

        if(send_done_endhost_data(out_data_map[out_target], out_bytes) == 1){
            target_send_done[out_target] = true;
            if(target_send_done[out_target] && target_recv_done[out_target]){
                flow_done = get_time_ns();
                flow_stats->entries[flow_stats->cnt++] = flow_done - flow_started;
                flow_started = get_time_ns();
                targets_done++;
            }
        }
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
        fprintf(resf, "RECV %d->%d:", it->first, my_rank);
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

    fprintf(statsf, "SENDSTATS: ");
    for(int i = 0; i < bulk_stats->cnt; i++) {
        fprintf(statsf, "%lu ", bulk_stats->entries[i]);
    }
    fprintf(statsf, "\n");

    fprintf(statsf, "FLOWSTATS: ");
    for(int i = 0; i < flow_stats->cnt; i++) {
        fprintf(statsf, "%lu ", flow_stats->entries[i]);
    }
    fprintf(statsf, "\n");

    fprintf(statsf, "RUNTIME: %lu\n", ended_run - started_run);

    fclose(statsf);
}

static void setup_endhost_stats() {
    bulk_stats = new delta_stats;
    bulk_stats->cnt = 0;
    flow_stats = new delta_stats;
    flow_stats->cnt = 0;
    out_missed = 0;
    inc_missed = 0;
}

static void free_endhost_stats() {
    delete bulk_stats;
    delete flow_stats;
}

int run_bulk_endhost() {
    setup_from_yaml();
    if(setup_mappings() != 0){
        fprintf(stderr, "Host %d: Failed to setup mappings\n", my_rank);
        return -1;
    }

    setup_endhost_stats();
    ts_request = new_request();
    inc_request = new_request();
    out_request = new_request();

    for(int i = 0; i < warmup_iters; i++){
        warmup_sync_connections();
        warmup_data_connections();
    }

    printf("my_rank=%d my_id=%d my_rotor=%d\tREADY\n", my_rank, my_id, my_rotor);

    transport->Barrier(sync_comm);
    started_run = get_time_ns();

    start_timeslot_recv();
    flow_started = get_time_ns();
    while (targets_done != num_targets) {
        if(check_for_timeslot()) {
            switch(update_timeslot()) {
                case 0:
                    record_stats_entry();
                    // terminate_transfers_and_wait();
                    // if(my_rotor != 0)
                    //     break;
                    start_new_transfers(1);
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
        if(check_transfer_states() == 1)
            start_new_transfers(0);
    }

endhost_done:
    ended_run = get_time_ns();

    if(targets_done == num_targets){
        printf("my_rank=%d\tCOMPLETED in %lu ns\tinc_missed=%d out_missed=%d\n",
               my_rank, ended_run - started_run,
               inc_missed, out_missed);
    } else {
       printf("my_rank=%d\tFAILED after %lu ns\tinc_missed=%d out_missed=%d completed=%d\n",
               my_rank, ended_run - started_run,
               inc_missed, out_missed, targets_done);
    }

    write_endhost_results();
    write_endhost_stats();

    terminate_transfers_and_wait();

    free_mappings();
    free_endhost_stats();

    free_request(ts_request);
    free_request(inc_request);
    free_request(out_request);

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
