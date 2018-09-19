#!/usr/bin/env python3
import argparse
import re
import os, os.path
import subprocess
import sys
import time

HOSTS={
    'b09-30': {
        'ranks': [0, 8, 16, 24],
        'role': 'endhost',
        'id': 0
    },
    'b09-32': {
        'ranks': [1, 9, 17, 25],
        'role': 'endhost',
        'id': 1
    },
    'b09-42': {
        'ranks': [2, 10, 18, 26],
        'role': 'endhost',
        'id': 2
    },
    'b09-38': {
        'ranks': [3, 11, 19, 27],
        'role': 'endhost',
        'id': 3
    },
    'b09-44': {
        'ranks': [4, 12, 20, 28],
        'role': 'endhost',
        'id': 4
    },
    'b09-40': {
        'ranks': [5, 13, 21, 29],
        'role': 'endhost',
        'id': 5
    },
    'b09-34': {
        'ranks': [6, 14, 22, 30],
        'role': 'endhost',
        'id': 6
    },
    'b09-36': {
        'ranks': [7, 15, 23, 31],
        'role': 'endhost',
        'id': 7
    },
    'b10-29': {
        'ranks': [32],
        'role': 'control',
        'id': 29
    },
    'b10-31': {
        'ranks': [33],
        'role': 'dummy',
        'id': 31
    },
}

DEFAULT_STATS_STR="/tmp/bulk_mpi_stats_rank_%d.txt"
DEFAULT_ENDHOST_STR="/tmp/endhost_stats_rank_%d.txt"
DEFAULT_RESULTS_STR="/tmp/endhost_results_rank_%d.txt"


def parse_args():
    parser = argparse.ArgumentParser(description="Gather and verify results for MPI Bulk experiment")

    parser.add_argument("-t", "--temp_dir", default="/tmp/mpi_bulk_stats", type=str,
                        help="Directory to locally store results before processing them")

    return parser.parse_args()


COPY_PROCS=[]
def wait_on_copies():
    for proc in COPY_PROCS:
        if proc.wait() != 0:
            raise Exception("Proc failed: %s" % str(proc.args))


def copy_from_remote(remote_host, remote_file, local_dest):
    proc = subprocess.Popen("scp %s:%s %s" % (remote_host, remote_file, local_dest), shell=True,
                            stdout=subprocess.PIPE)
    COPY_PROCS.append(proc)


def copy_run_files(dest_dir):
    stats_files = []
    endhost_files = []
    results_files = []
    for host in HOSTS:
        for rank in HOSTS[host]["ranks"]:
            rname = DEFAULT_STATS_STR % rank
            lname = os.path.join(dest_dir, os.path.basename(rname))
            copy_from_remote(host, rname, lname)
            stats_files.append((rank, lname))

            if HOSTS[host]["role"] == "endhost":
                rname = DEFAULT_ENDHOST_STR % rank
                lname = os.path.join(dest_dir, os.path.basename(rname))
                copy_from_remote(host, rname, lname)
                endhost_files.append((rank, lname))

                rname = DEFAULT_RESULTS_STR % rank
                lname = os.path.join(dest_dir, os.path.basename(rname))
                copy_from_remote(host, rname, lname)
                results_files.append((rank, lname))
            time.sleep(0.1)

    wait_on_copies()

    return (stats_files, endhost_files, results_files)


def verify_sha_hashes(base_map, res_map):
    for src, dst in base_map:
        key = (src, dst)
        if key not in res_map:
            raise ValueError("Couldn't find %s in res_map" % str(key))

        for sha in base_map[key]:
            if sha not in res_map[key]:
                raise ValueError("Hash value %s not in res_map for key %s" % (sha, str(key)))


def load_sha_hashes(results_files):
    send_map = {}
    recv_map = {}

    for rank, lfname in results_files:
        with open(lfname, "r") as f:
            for line in f:
                match = re.match("(?P<dir>SEND|RECV)\s*(?P<src>\d+)->(?P<dst>\d+):\s*(?P<shas>.*)", line.strip())
                vals = match.groupdict()
                shas = set(vals["shas"].strip().split())
                if vals["dir"] == "SEND":
                    send_map[(int(vals["src"]), int(vals["dst"]))] = shas
                elif vals["dir"] == "RECV":
                    recv_map[(int(vals["src"]), int(vals["dst"]))] = shas
                else:
                    raise ValueError("Line %s is invalid" % line)

    return (send_map, recv_map)


if __name__ == "__main__":
    args = parse_args()

    if not os.path.exists(args.temp_dir):
        os.mkdir(args.temp_dir)

    print("Copying stats files...")
    stats_files, endhost_files, results_files = copy_run_files(args.temp_dir)
    print("Loading SHA256 hashes...")
    send_map, recv_map = load_sha_hashes(results_files)
    print("Verifying hashes...")
    verify_sha_hashes(send_map, recv_map)
    verify_sha_hashes(recv_map, send_map)
    print("Done!")
