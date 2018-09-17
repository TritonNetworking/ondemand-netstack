#include <random>
#include <cstdlib>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "fake_data.h"

void free_endhost_data(struct fake_endhost_data* edata) {
    if (edata == NULL)
        return;

    for(int i = 0; i < edata->num_bufs; i++){
        if (edata->data_arrs[i] != NULL)
            delete edata->data_arrs[i];
    }

    delete edata;
}

int reset_endhost_data(struct fake_endhost_data* edata) {
    edata->read_index = 0;
    edata->read_offset = 0;
    edata->write_index = 0;
    edata->write_offset = 0;
    edata->warmups_done = 0;

    for(int i = 0; i < edata->num_bufs; i++) {
        if(edata->data_arrs[i] == NULL)
            return -1;
        memset(edata->data_arrs[i], 0,
               sizeof(char) * edata->buf_size);
    }

    return 0;
}

int fill_endhost_data(struct fake_endhost_data* edata) {
    if(edata == NULL)
        return -1;

    for(int i = 0; i < edata->num_bufs; i++) {
        if(edata->data_arrs[i] == NULL)
            return -1;
        for (int k = 0; k < edata->buf_size; k++)
            edata->data_arrs[i][k] = (char)(rand() % 256);
    }

    edata->write_index = edata->num_bufs;
    edata->write_offset = -1;

    return 0;
}

struct fake_endhost_data* allocate_fake_data(
        int src_id,
        int dst_id,
        int buf_size,
        int num_bufs)
{
    struct fake_endhost_data *edata = new struct fake_endhost_data;
    if (edata == NULL)
        goto fail_alloc;

    edata->src_id = src_id;
    edata->dst_id = dst_id;
    edata->buf_size = buf_size;
    edata->num_bufs = num_bufs;

    edata->data_arrs = new char*[num_bufs];
    if(edata->data_arrs == NULL)
        goto fail_alloc;
    memset(edata->data_arrs, 0, sizeof(char*) * num_bufs);

    for (int i = 0; i < num_bufs; i++) {
        edata->data_arrs[i] = new char[buf_size];
        if(edata->data_arrs[i] == NULL){
            goto fail_alloc;
        }
    }

    if(reset_endhost_data(edata) != 0)
        goto fail_alloc;

    return edata;

fail_alloc:
    free_endhost_data(edata);
    return NULL;
}
