#ifndef BULKMPI_FAKE_DATA_H_
#define BULKMPI_FAKE_DATA_H_

#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define WARMUP_BYTES_SIZE 256

struct fake_endhost_data {
    char **data_arrs;
    int read_index, read_offset;
    int write_index, write_offset;
    int buf_size, num_bufs;
    int src_id, dst_id;
    int warmups_done;
};

void free_endhost_data(struct fake_endhost_data* edata);
int reset_endhost_data(struct fake_endhost_data* edata);
int fill_endhost_data(struct fake_endhost_data* edata);
struct fake_endhost_data* allocate_fake_data(int src_id,
                                             int dst_id,
                                             int buf_size,
                                             int num_bufs);
int send_next_endhost_data(struct fake_endhost_data* edata,
                           char **target_buf,
                           int *max_bytes,
                           int max_warmups);
int send_done_endhost_data(struct fake_endhost_data* edata,
                           int bytes_done,
                           int max_warmups);
int recv_next_endhost_data(struct fake_endhost_data* edata,
                           char **target_buf,
                           int *max_bytes,
                           int max_warmups);
int recv_done_endhost_data(struct fake_endhost_data* edata,
                           int bytes_done,
                           int max_warmups);

#endif
