#ifndef BULKMPI_STATS_H_
#define BULKMPI_STATS_H_

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "utils.h"

#define NUM_STAT_ENTRIES 100000

extern uint64_t *global_stats;
extern int global_stats_cnt;

static int allocate_global_stats() {
    global_stats = new uint64_t[NUM_STAT_ENTRIES];
    memset(global_stats, 0, sizeof(uint64_t) * NUM_STAT_ENTRIES);
    global_stats_cnt = 0;

    if (global_stats == NULL)
        return -1;
    return 0;
}

static void free_global_stats() {
    if(global_stats != NULL)
        delete global_stats;
}

static inline int record_stats_entry() {
    if (global_stats == NULL)
        return -1;
    if (global_stats_cnt >= NUM_STAT_ENTRIES)
        return 1;

    global_stats[global_stats_cnt++] = get_time_ns();
    return 0;
}

static void print_global_stats(int node_id) {
    printf("stats\tnode_id: %d\n", node_id);
    for(int i = 2; i < global_stats_cnt; i++)
        printf("%lu ", global_stats[i] - global_stats[i-1]);
    printf("\n");
}

static int write_global_stats(int node_id, const char* filename) {
    FILE *f = fopen(filename, "w");
    if (f == NULL) {
        fprintf(stderr, "Failed to write stats to file %s\n", filename);
        return -1;
    }

    fprintf(f, "stats\tnode_id: %d\n", node_id);
    for(int i = 2; i < global_stats_cnt; i++)
        fprintf(f, "%d ", ((int)(global_stats[i] - global_stats[i-1])) - 250000);
    fprintf(f, "\n");
    fclose(f);

    return 0;
}

#endif
