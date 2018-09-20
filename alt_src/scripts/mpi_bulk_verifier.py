#!/usr/bin/env python3
import argparse
import re
import os, os.path
import subprocess
import sys
import time

import numpy as np
import misc_python


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
    parser.add_argument("-d", "--endhosts_graph", default=None, type=str,
                        help="Output a graph of the send deltas at endhosts")
    parser.add_argument("-c", "--control_graph", default=None, type=str,
                        help="Output a graph of the send deltas at control & dummy")
    parser.add_argument("-s", "--sends_graph", default=None, type=str,
                        help="Plot a graph of send data for each endhost")
    parser.add_argument("-f", "--flows_graph", default=None, type=str,
                        help="Output a graph of FCTs for bulk traffic")
    parser.add_argument("-r", "--runtime_data", default=False, action="store_true",
                        help="Print stats on the runtime completion of each endhost")
    parser.add_argument("--skip_copy", default=False, action="store_true",
                        help="Skip copying files from remote hosts and use local copies")

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


def copy_run_files(dest_dir, skip_copy=False):
    stats_files = []
    endhost_files = []
    results_files = []
    for host in HOSTS:
        for rank in HOSTS[host]["ranks"]:
            rname = DEFAULT_STATS_STR % rank
            lname = os.path.join(dest_dir, os.path.basename(rname))
            if not skip_copy:
                copy_from_remote(host, rname, lname)
            stats_files.append((rank, lname))

            if HOSTS[host]["role"] == "endhost":
                rname = DEFAULT_ENDHOST_STR % rank
                lname = os.path.join(dest_dir, os.path.basename(rname))
                if not skip_copy:
                    copy_from_remote(host, rname, lname)
                endhost_files.append((rank, lname))

                rname = DEFAULT_RESULTS_STR % rank
                lname = os.path.join(dest_dir, os.path.basename(rname))
                if not skip_copy:
                    copy_from_remote(host, rname, lname)
                results_files.append((rank, lname))
            if not skip_copy:
                time.sleep(0.1)

    if not skip_copy:
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


def load_bulk_stats(stats_files):
    ts_deltas = {}
    for rank, sfname in stats_files:
        with open(sfname, "r") as f:
            vals = f.read().strip().split()
            ts_deltas[rank] = [int(v) for v in vals]
    return ts_deltas


def load_endhost_stats(endhost_files):
    send_deltas = {}
    flow_deltas = {}
    runtimes = {}
    for rank, efname in endhost_files:
        with open(efname, "r") as f:
            for line in f:
                if line.startswith("SENDSTATS"):
                    send_deltas[rank] = [int(v) for v in
                                         line.split(":")[1].strip().split()]
                elif line.startswith("FLOWSTATS"):
                    flow_deltas[rank] = [int(v) for v in
                                         line.split(":")[1].strip().split()]
                elif line.startswith("RUNTIME"):
                    runtimes[rank] = int(line.split(":")[1].strip())
                else:
                    continue
    return (send_deltas, flow_deltas, runtimes)


def plot_deltas_graph(ts_deltas, out_fname):
    exps = {str(r): {"data": ts_deltas[r]}
            for r in ts_deltas}
    misc_python.plot_cdf_set_all(exps, out_fname,
                                 title="Deltas graph",
                                 xlabel="Nanoseconds",
                                 ylabel="CDF",
                                 ylim=(0,1.0),
                                 legend_title="MPI Rank")


def print_runtime_info(runtimes):
    print("Runtime stats:")
    runtime_data = [runtimes[r] for r in runtimes]
    print("Mean: %.2f\tMedian: %.2f\tMin: %d\tMax:%d" % (
          np.mean(runtime_data), np.median(runtime_data),
          min(runtime_data), max(runtime_data)))


if __name__ == "__main__":
    args = parse_args()

    if not os.path.exists(args.temp_dir):
        os.mkdir(args.temp_dir)

    print("Copying stats files...")
    stats_files, endhost_files, results_files = copy_run_files(args.temp_dir, args.skip_copy)

    print("Loading SHA256 hashes...")
    send_map, recv_map = load_sha_hashes(results_files)
    print("Verifying hashes...")
    verify_sha_hashes(send_map, recv_map)
    verify_sha_hashes(recv_map, send_map)

    control_ranks = [r for host in HOSTS for r in HOSTS[host]["ranks"]
                     if HOSTS[host]["role"] == "control" or HOSTS[host]["role"] == "dummy"]
    endhost_ranks = [r for host in HOSTS for r in HOSTS[host]["ranks"]
                     if HOSTS[host]["role"] == "endhost"]

    if args.control_graph or args.endhosts_graph:
        print("Loading bulk stats...")
        ts_deltas = load_bulk_stats(stats_files)
        if args.control_graph:
            print("Writing control graph to %s..." % args.control_graph)
            plot_deltas_graph({r: ts_deltas[r] for r in ts_deltas if r in control_ranks},
                              args.control_graph)
        if args.endhosts_graph:
            print("Writing deltas graph to %s..." % args.endhosts_graph)
            plot_deltas_graph({r: ts_deltas[r] for r in ts_deltas if r in endhost_ranks},
                              args.endhosts_graph)

    if args.flows_graph or args.sends_graph or args.runtime_data:
        print("Loading endhost stats...")
        send_deltas, flow_deltas, runtimes = load_endhost_stats(endhost_files)
        if args.flows_graph:
            print("Writing flow graph to %s..." % args.flows_graph)
            plot_deltas_graph(flow_deltas, args.flows_graph)
        if args.sends_graph:
            print("Writing sends graph to %s..." % args.sends_graph)
            plot_deltas_graph(send_deltas, args.sends_graph)
        if args.runtime_data:
            print_runtime_info(runtimes)
        import pdb; pdb.set_trace()

    print("Done!")
