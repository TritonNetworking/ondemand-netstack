#!/bin/bash
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

taskset --cpu-list 11,51,13,53,15,55,17,57,19,59,21,61 \
/usr/mpi/gcc/openmpi-3.1.1rc1/bin/mpirun \
    -np 10 --hostfile config/test_hfile --rankfile config/test_rfile --report-bindings \
    -mca coll_hcoll_enable 0 \
    -mca pml ob1 --mca btl openib,self,vader \
    --mca btl_openib_cpc_include rdmacm \
    --mca btl_openib_rroce_enable 1 \
    -mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 \
    ./build/src/bulk_mpi $DIR/config/test.yaml 12345
