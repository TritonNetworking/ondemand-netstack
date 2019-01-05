#!/bin/bash

HOSTFILE=hosts.config
PROCS_PER_HOST=4

control=$(head -1 $HOSTFILE)
dummy=$(head -2 $HOSTFILE | tail -1)
HOST_COUNT=$(wc -l < $HOSTFILE)
ENDHOST_COUNT=$(( HOST_COUNT - 2 ))

host=$(hostname -s)
lineno=$(awk "/$host/{print NR}" $HOSTFILE)
if [ "$host" = "$control" ]; then
    id="control"
    rank=$(( ENDHOST_COUNT * PROCS_PER_HOST ))
    echo >&2 "Hello from $host, rank = $rank, id = $id."
elif [ "$host" = "$dummy" ]; then
    id="dummy"
    rank=$(( ENDHOST_COUNT * PROCS_PER_HOST + 1 ))
    echo >&2 "Hello from $host, rank = $rank, id = $id."
else
    id=$(( lineno - 3 ))
    for (( i = 0; i < $PROCS_PER_HOST; i++ )); do
        rank=$(( id + i * ENDHOST_COUNT ))
        echo >&2 "Hello from $host, rank = $rank, id = $id."
    done
fi


