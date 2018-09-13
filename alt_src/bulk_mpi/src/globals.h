#ifndef BULKMPI_GLOBALS_H_
#define BULKMPI_GLOBALS_H_

#define DATA_COLOR 0xDA1AC0D3
#define SYNC_COLOR 0xBA3BC0D3

#define SYNC_PKT_SIZE 9

extern int world_size, data_world_size, sync_world_size;
extern int world_rank, data_rank, sync_rank;
extern MPI_Comm data_comm, sync_comm;

extern char* config_file;
extern uint64_t base_time;

extern YAML::Node bulk_config;

#endif