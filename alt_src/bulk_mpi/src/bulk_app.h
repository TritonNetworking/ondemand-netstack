#include <cstdlib>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "yaml-cpp/yaml.h"
#include "globals.h"
#include "utils.h"
#include "run_funcs.h"
#include "stats.h"
#include "transport.h"

char* config_file;
uint64_t base_time;

YAML::Node bulk_config;
TransportBase *transport;
CommBase *data_comm, *sync_comm;

uint64_t *global_stats = NULL;
int global_stats_cnt = -1;

extern void init_transport();

int load_yaml_config(char* filename) {
    bulk_config = YAML::LoadFile(config_file);
    return 0;
}

int parse_args(int argc, char** argv) {
    if (argc < 3)
        return -1;

    // Config file
    struct stat fstats;
    if (stat(argv[1], &fstats)) {
        fprintf(stderr, "'%s' is not a valid filename\n", argv[1]);
        return -1;
    }
    config_file = argv[1];

    // Base time
    errno = 0;
    base_time = strtoull(argv[2], NULL, 0);
    if (errno != 0){
        fprintf(stderr, "'%s' is not a valid base time\n", argv[2]);
        return -1;
    }

    return 0;
}

int usage() {
    fprintf(stderr, "Usage: ./bulk_mpi <config_file> <base_time>");
    return -1;
}

void print_test_data() {
    int world_rank = transport->get_world_rank();
    int data_rank = transport->get_data_rank();
    int sync_rank = transport->get_sync_rank();
    printf("base_time: %lu\tworld_rank: %d\tdata_rank: %d\tsync_rank: %d\n",
           base_time, world_rank, data_rank, sync_rank);
    if(bulk_config["test_value"])
        printf("The line is:\n%s\n", bulk_config["test_value"].as<std::string>().c_str());
    else
        printf("Error: no line found!\n");
}

int run_designated_task(int world_rank) {
    // Get config node
    YAML::Node rank_to_id = load_or_abort(bulk_config, "rank_to_id");
    if(rank_to_id.Type() != YAML::NodeType::Map){
        fprintf(stderr, "rank_to_id is invalid\n");
        return -1;
    }
    YAML::Node this_id = rank_to_id[world_rank];
    if(this_id.Type() != YAML::NodeType::Scalar){
        fprintf(stderr, "rank_to_id for node %d is invalid\n", world_rank);
        return -1;
    }

    // Get the type of thing this node is and call the function for it
    std::string id_str = this_id.as<std::string>();
    // wait_for_debugger();
    if(id_str.compare("control") == 0){
        run_as_control();
        // printf("world_rank=%d\tcontrol\n", world_rank);
    } else
    if (id_str.compare("dummy") == 0) {
        run_as_dummy();
        // printf("world_rank=%d\tdummy\n", world_rank);
    } else {
        // uint id_val = this_id.as<uint>();
        run_as_endhost(world_rank);
        // printf("world_rank=%d\tid_val=%u\n", world_rank, id_val);
    }

    printf("world_rank=%d did the thing!\n", world_rank);

    return 0;
}

int main(int argc, char** argv) {
    if(parse_args(argc, argv))
        return usage();

    if(load_yaml_config(config_file)){
        fprintf(stderr, "Failed to load %s\n", config_file);
        return -1;
    }

    init_transport();
    transport->Setup(data_comm, sync_comm);

    allocate_global_stats();

    int world_rank = transport->get_world_rank();
    int ret = run_designated_task(world_rank);

    char stats_fname[128];
    snprintf(stats_fname, 128, "/tmp/bulk_mpi_stats_rank_%d.txt", world_rank);
    write_global_stats(world_rank, stats_fname);
    free_global_stats();

    transport->Teardown();
    delete data_comm;
    delete sync_comm;
    delete transport;

    return ret;
}
