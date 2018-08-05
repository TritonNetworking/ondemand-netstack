#!/usr/bin/env bash

cd "$(dirname "$0")"

# Check if MPI environment is loaded
if ! [ -x "$(command -v mpirun)" ]; then
    source ../setup-hpcx.sh
fi

source ./config

# Default command line argument
YALLA=false
PROGRAM="mpi_daemon"

# Parse command line argument
while [[ $# -gt 0 ]]
do
key="$1"

case $key in
    -y|--yalla)
        YALLA=true
        shift # past argument
        ;;
    -p|--program)
        PROGRAM="$2"
        if [[ $PROGRAM != "mpi_daemon" ]]; then
            >&2 echo "Invalid program \"$PROGRAM\""
            exit 2
        fi
        shift # past argument
        shift # past value
        ;;
    *)
        >&2 echo "Unrecognized option $key"
        exit 2
esac
done

# Config
hostfile=$HOSTS_PATH
hosts=$(cat $hostfile| paste -s -d "," -)
np=$(cat $hostfile | wc -l)

# Machine-dependent variables
hcaid=$(ibv_devinfo | grep hca_id | awk '{ print $2 }')

# From https://community.mellanox.com/docs/DOC-3076#jive_content_id_Running_MPI
#: '
HCAS="$hcaid:1"
FLAGS+="-mca btl_openib_warn_default_gid_prefix 0 "
FLAGS+="-mca btl_openib_warn_no_device_params_found 0 "
FLAGS+="--report-bindings --allow-run-as-root -bind-to core "
FLAGS+="-mca coll_fca_enable 0 -mca coll_hcoll_enable 0 "
if $YALLA; then
    FLAGS+="-mca pml yalla "
fi
FLAGS+="-mca mtl_mxm_np 0 -x MXM_TLS=ud,shm,self -x MXM_RDMA_PORTS=$HCAS "
FLAGS+="-x MXM_LOG_LEVEL=ERROR -x MXM_IB_PORTS=$HCAS "
FLAGS+="-x MXM_IB_MAP_MODE=round-robin -x MXM_IB_USE_GRH=y "
#'

# From Max
FLAGS+="-mca btl_openib_receive_queues P,65536,256,192,128:S,128,256,192,128:S,2048,1024,1008,64:S,12288,1024,1008,64:S,65536,1024,1008,64 "

run_mpi_daemon() {
    execname=$MPI_DAEMON_EXECPATH
    mpirun -np $np --host $hosts $FLAGS $execname
}

# Launch MPI job
set -e

case $PROGRAM in
    mpi_daemon)
        run_mpi_daemon
        ;;
    *)
        >&2 echo "Program not specified ..."
        exit 2
esac

