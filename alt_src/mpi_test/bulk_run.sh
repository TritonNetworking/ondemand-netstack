#!/bin/bash

/usr/mpi/gcc/openmpi-3.1.1rc1/bin/mpirun \
    -np 5 --hostfile blk_hfile --rankfile blk_rfile --report-bindings \
    -mca coll_hcoll_enable 0 \
    -mca pml ob1 --mca btl openib,self,vader \
    --mca btl_openib_cpc_include rdmacm \
    --mca btl_openib_rroce_enable 1 \
    -mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 \
    ./src_rotor_test
    # ./hello_world
    # /usr/mpi/gcc/openmpi-3.1.1rc1/tests/osu-micro-benchmarks-5.3.2/osu_latency
