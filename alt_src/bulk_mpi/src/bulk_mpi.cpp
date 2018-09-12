#include <cstdlib>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "yaml-cpp/yaml.h"

#define DATA_COLOR 0xDA1AC0D3
#define SYNC_COLOR 0xBA3BC0D3

static int world_size, data_world_size, sync_world_size;
static int world_rank, data_rank, sync_rank;
static MPI_Comm data_comm, sync_comm;

static char* config_file;
static uint64_t base_time;

static YAML::Node bulk_config;

void MPI_Setup() {
    MPI_Init(NULL, NULL);

    // Setup global MPI
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    // Setup sync MPI
    MPI_Comm_split(MPI_COMM_WORLD, SYNC_COLOR, world_rank, &sync_comm);
    MPI_Comm_size(sync_comm, &sync_world_size);
    MPI_Comm_rank(sync_comm, &sync_rank);

    // Setup data MPI
    MPI_Comm_split(MPI_COMM_WORLD, DATA_COLOR, world_rank, &data_comm);
    MPI_Comm_size(data_comm, &data_world_size);
    MPI_Comm_rank(data_comm, &data_rank);
}

void MPI_Teardown() {
    MPI_Finalize();
}

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
    printf("world_rank: %d\tdata_rank: %d\tsync_rank: %d\n", world_rank, data_rank, sync_rank);
    if(bulk_config["test_value"])
        printf("The line is:\n%s\n", bulk_config["test_value"].as<std::string>().c_str());
    else
        printf("Error: no line found!\n");
}

int main(int argc, char** argv) {
    if(parse_args(argc, argv))
        return usage();

    if(load_yaml_config(config_file)){
        fprintf(stderr, "Failed to load %s\n", config_file);
        return -1;
    }

    MPI_Setup();

    // DO THE THING
    print_test_data();

    MPI_Teardown();

    return 0;
}
