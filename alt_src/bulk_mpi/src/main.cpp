#include <cstdlib>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>

#include "yaml_config.h"
#include "run_funcs.h"

// dummy struct
struct rdma_qp {

};

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

int run_designated_task(int world_rank, struct yaml_config config) {
    auto it = config.rank_to_id.find(world_rank);
    if (it == config.rank_to_id.end()) {
        fprintf(stderr, "rank not found in config.\n");
        return -1;
    }

    std::string id = it->second;
    if (id == "control")
        run_as_control();
    else if (id == "dummy")
        run_as_dummy();
    else
        run_as_endhost(world_rank);

    return 0;
}

int main(int argc, char *argv[]) {
    if(parse_args(argc, argv))
        return usage();

    config = load_config(config_file);
    int retval = run_designated_task(world_rank, config);

    return retval;
}
