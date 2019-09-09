#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>

#include "yaml_config.h"
#include "run_funcs.h"

char* config_file;
struct yaml_config config;
uint64_t base_time;
int world_rank;

int parse_args(int argc, char** argv) {
    if (argc < 4)
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

    // World rank
    errno = 0;
    world_rank = strtoull(argv[3], NULL, 0);
    if (errno != 0){
        fprintf(stderr, "'%s' is not a valid rank\n", argv[3]);
        return -1;
    }

    return 0;
}

int usage() {
    fprintf(stderr, "Usage: ./bulk_mpi <config_file> <base_time> <world_rank>");
    return -1;
}

int run_designated_task(int rank, struct yaml_config config) {
    auto it = config.rank_to_id.find(rank);
    if (it == config.rank_to_id.end()) {
        fprintf(stderr, "rank not found in config.\n");
        return -1;
    }

    if (rank == config.control_rank)
        run_as_control();
    else if (rank == config.dummy_rank)
        run_as_dummy();
    else
        run_as_endhost(rank);

    return 0;
}

int main(int argc, char *argv[]) {
    if(parse_args(argc, argv))
        return usage();

    config = load_config(config_file);
    int retval = run_designated_task(world_rank, config);

    return retval;
}
