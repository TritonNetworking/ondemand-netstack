#!/usr/bin/env python
import argparse
import multiprocessing
import subprocess
import os, os.path
import sys

DEFAULT_IRQ_PATH    = '/proc/irq'
DEFAULT_AFF_FILE    = 'smp_affinity'
DEFAULT_LCORE_IDS   = [14,34,15,35,16,36,17,37]
DEFAULT_FREQ_FILE   ="/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor"

LOWER_CPU_LIMIT=32

def parse_args():
    parser = argparse.ArgumentParser(description="Setup single host for TDMA BESS experiment")
    parser.add_argument("-d", "--disable", action="store_true", default=False,
                        help="Revert hosts to original state")
    parser.add_argument("-l", "--lcore_ids", type=int, default=DEFAULT_LCORE_IDS, nargs='+',
                        help="CPU IDs to mask interrupts for")
    # parser.add_argument("--irq_path", type=str, default=DEFAULT_IRQ_PATH,
    #                     help="Path to IRQ /proc directory")
    # parser.add_argument("--aff_file", type=str, default=DEFAULT_AFF_FILE,
    #                     help="SMP affinity filename in IRQ proc directory")
    args = parser.parse_args()
    return args


def setup_freq_scaling(disable=False):
    if disable:
        method = 'powersave'
    else:
        method = 'performance'

    for i in range(multiprocessing.cpu_count()):
        freq_file = DEFAULT_FREQ_FILE % i
        try:
            subprocess.check_call("echo %s > %s" % (method, freq_file),
                                  shell=True,
                                  stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE)
        except subprocess.CalledProcessError:
            print >> sys.stderr, "Error writing %s. Unchanged" % freq_file


def setup_service(service, disable=False):
    if disable:
        method = 'start'
    else:
        method = 'stop'

    try:
        subprocess.check_call("service %s %s" % (service, method),
                              shell=True,
                              stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE)
    except subprocess.CalledProcessError:
        print >> sys.stderr, "Error modifying %s. Unchanged" % service


def mask_irqs(cpu_ids, irq_path=DEFAULT_IRQ_PATH, aff_file=DEFAULT_AFF_FILE, disable=False):
    def get_enable_mask(cpu_ids):
        if not cpu_ids:
            return None
        mask = int(0)
        for cpu in cpu_ids:
            o = 1 << cpu
            mask = mask | o
        return mask

    def get_disable_mask(cpu_ids):
        if not cpu_ids:
            return None
        mask = 0xffffffff
        for cpu in cpu_ids:
            o = ~(1 << cpu)
            mask = mask & o
        return mask

    def filter_cpu_ids(cpu_ids):
        lower_cpus = []
        upper_cpus = []
        for cpu in cpu_ids:
            if cpu < LOWER_CPU_LIMIT:
                lower_cpus.append(cpu)
            else:
                upper_cpus.append(cpu)
        return (lower_cpus, upper_cpus)

    lower_cpus, upper_cpus = filter_cpu_ids(cpu_ids)
    if disable:
        lower_mask = get_enable_mask(lower_cpus)
        upper_mask = get_enable_mask(upper_cpus)
    else:
        lower_mask = get_disable_mask(lower_cpus)
        upper_mask = get_disable_mask(upper_cpus)

    for path, dirs, files in os.walk(irq_path):
        if aff_file in files:
            irq_file = os.path.join(path, aff_file)

            with open(irq_file, 'r') as f:
                affinity = f.read()
                affinity = affinity[:-1]  # strip newline

            affinity_parts = affinity.split(',')
            upper, lower = (affinity_parts[-2], affinity_parts[-1])
            hi_bits = int(upper, 16)
            lo_bits = int(lower, 16)

            if lower_mask:
                if disable:
                    lo_bits = lo_bits | lower_mask
                else:
                    lo_bits = lo_bits & lower_mask
            if upper_mask:
                if disable:
                    hi_bits = hi_bits | upper_mask
                else:
                    hi_bits = hi_bits & upper_mask

            wb = '%x,%x\n' % (hi_bits, lo_bits)
            wbt = [','.join(affinity_parts[:-2]), wb] if affinity_parts[:-2] else [wb]
            wbf = ','.join(wbt)
            try:
                with open(irq_file, 'w') as f:
                    f.write(wbf)
            except IOError:
                print >> sys.stderr, "Error writing %s. Unchanged" % irq_file


def main(args):
    # setup_freq_scaling(disable=args.disable)
    setup_service('cron', disable=args.disable)
    setup_service('atd', disable=args.disable)
    setup_service('iscsid', disable=args.disable)
    setup_service('irqbalance', disable=args.disable)
    mask_irqs(args.lcore_ids,
              # irq_path=args.irq_path,
              # aff_file=args.aff_file,
              disable=args.disable)


if __name__ == "__main__":
    args = parse_args()
    main(args)
