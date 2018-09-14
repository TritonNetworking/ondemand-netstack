#include <cstdlib>
#include <iostream>
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

static uint64_t now, next_trigger_real, next_trigger_virt;
static uint64_t period_ns, guard_time, prealloc_delay;
static uint64_t min_exp_delay, exp_duration_ns, exp_end_time;
static uint link_rate_gbps;
static uint magic, done_magic;
static int num_hosts, num_rotors;

static int ts_index, num_ts;
static std::vector<uint> ts_order;
static std::vector<int> next_targets;
static YAML::Node timeslots, mappings;

static YAML::Node id_to_rank, rank_to_id, rank_to_rotor;
static int dummy_host;

static YAML::Node next_ts;
static uint next_ts_id;
static uint64_t slot_delay_ns, byte_allocation_ns, bytes_to_send;
static int affected_rotor, rotor_state;

static char send_buf[SYNC_PKT_SIZE];

inline void wait_until(uint64_t target) {
    while(now < target)
        now = get_time_ns();
}

void setup_from_yaml() {
    period_ns = load_or_abort(bulk_config, "total_period_ns").as<uint64_t>();
    guard_time = load_or_abort(bulk_config, "guard_time_ns").as<uint64_t>();
    prealloc_delay = load_or_abort(bulk_config, "prealloc_delay_ns").as<uint64_t>();

    magic = load_or_abort(bulk_config, "magic").as<uint>();
    done_magic = load_or_abort(bulk_config, "done_magic").as<uint>();

    link_rate_gbps = load_or_abort(bulk_config, "link_rate_gbps").as<uint>();
    num_hosts = load_or_abort(bulk_config, "num_hosts").as<int>();
    num_rotors = load_or_abort(bulk_config, "num_rotors").as<int>();

    id_to_rank = load_or_abort(bulk_config, "id_to_rank");
    rank_to_id = load_or_abort(bulk_config, "rank_to_id");
    rank_to_rotor = load_or_abort(bulk_config, "rank_to_rotor");
    dummy_host = load_or_abort(id_to_rank, "dummy").as<int>();

    ts_order = load_or_abort(bulk_config, "timeslot_order").as<std::vector<uint>>();
    num_ts = (int)ts_order.size();
    ts_index = 0;
    timeslots = load_or_abort(bulk_config, "timeslots");
    mappings = load_or_abort(bulk_config, "mappings");

    exp_duration_ns = load_or_abort(bulk_config, "exp_duration_ms").as<uint64_t>() * 1000000ul;
    min_exp_delay = load_or_abort(bulk_config, "min_exp_delay_ns").as<uint64_t>();

    now = get_time_ns();
    next_trigger_real = now;
    while(next_trigger_real < (now + min_exp_delay)) {
        now = get_time_ns();
        next_trigger_real += period_ns;
    }
    exp_end_time = now + exp_duration_ns;
}

void load_next_timeslot() {
    while(next_trigger_virt <= now){
        next_ts_id = ts_order[ts_index];
        next_ts = timeslots[next_ts_id];
        ts_index = (ts_index + 1) % num_ts;

        slot_delay_ns = next_ts["slot_delay_us"].as<uint64_t>() * 1000ul;
        byte_allocation_ns = next_ts["byte_allocation_us"].as<uint64_t>() * 1000ul;
        // This'll just be calculated at the endhosts.
        // if(byte_allocation_ns)
        //     bytes_to_send = get_bytes_for_time(byte_allocation_ns - guard_time_ns,
        //                                        link_rate_gbps,
        //                                        (uint)num_hosts);
        // else
        //     bytes_to_send = 0;

        affected_rotor = next_ts["affected_rotor"].as<int>();
        rotor_state = next_ts["affected_rotor"].as<int>();

        next_trigger_real += slot_delay_ns;
        next_trigger_virt = next_trigger_real - prealloc_delay;

        next_targets.clear();
        for(int i = 0; i < num_hosts; i++){
            int target = id_to_rank[i][affected_rotor].as<int>();
            next_targets.push_back(target);
        }

        now = get_time_ns();
    }

    if(make_pkt(send_buf, SYNC_PKT_SIZE, magic, next_ts_id, byte_allocation_ns)){
        fprintf(stderr, "Failed to make sync packet at control host\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }
}

void send_sync_packets() {
    if (rotor_state == 0) {
        MPI_Request to_dummy;
        int to_dummy_completed = 0;
        MPI_Issend(send_buf, SYNC_PKT_SIZE, MPI_CHAR, dummy_host,
                   0, sync_comm, &to_dummy);
        while(!to_dummy_completed)
            MPI_Test(&to_dummy, &to_dummy_completed, MPI_STATUS_IGNORE);
    }
    // A new rotor came up, along with a new one going down
    else {
        MPI_Request to_endhosts[num_hosts];
        int to_endhosts_completed[num_hosts];
        int endhosts_done = 0;

        for(int i = 0; i < num_hosts; i++){
            int target = next_targets[i];
            MPI_Issend(send_buf, SYNC_PKT_SIZE, MPI_CHAR, target,
                       0, sync_comm, &to_endhosts[i]);
        }

        while(!endhosts_done) {
            endhosts_done = 1;
            for(int i = 0; i < num_hosts; i++){
                if(to_endhosts_completed[i] == 0){
                    MPI_Test(&to_endhosts[i], &to_endhosts_completed[i],
                             MPI_STATUS_IGNORE);
                    if(to_endhosts_completed[i] == 0)
                        endhosts_done = 0;
                }
            }
        }
    }
}

void send_final_packets() {
    if(make_pkt(send_buf, SYNC_PKT_SIZE, done_magic, 0, 0)){
        fprintf(stderr, "Failed to make final sync packet at control host\n");
        MPI_Abort(MPI_COMM_WORLD, -1);
    }

    MPI_Send(send_buf, SYNC_PKT_SIZE, MPI_CHAR,
             dummy_host, 0, sync_comm);
    for(int i = 0; i < num_hosts; i++) {
        for (int j = 0; j < num_rotors; j++) {
            int target = id_to_rank[i][j].as<int>();
            MPI_Send(send_buf, SYNC_PKT_SIZE, MPI_CHAR,
             target, 0, sync_comm);
        }
    }
}

int run_as_control() {
    printf("Starting control host...\n");

    setup_from_yaml();

    while(next_trigger_real < exp_end_time) {
        load_next_timeslot();
        // printf("Loaded ts\n");
        wait_until(next_trigger_virt);
        // printf("Wait done\n");
        send_sync_packets();
        // printf("Sent sync\n");
        record_stats_entry();
        now = get_time_ns();
        // printf("loop done\n");
    }

    // printf("Sending final packets\n");
    send_final_packets();

    return 0;
}