import argparse
import csv
import re
import os, os.path
import sys
import time
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def calculate_pkt_len(pkt_size, link_rate_gbps):
    if not link_rate_gbps:
        return 0
    return int(round(float((pkt_size+24)*8) / float(link_rate_gbps)))


def get_cmap(n, name='hsv'):
    # cmap = plt.cm.get_cmap(name, n)
    # return [cmap(i) for i in range(n)]
    if n <= 1:
        return ["#000000"]
    return [plt.cm.get_cmap(name)(1. * i / (n - 1)) for i in range(n)]


def load_results(files):
    results = {}
    for file in files:
        with open(file, "r") as f:
            reader = csv.DictReader(f)
            rows = [row for row in reader]
            results[file] = rows
    return results


def get_num_lines(results):
    total = 0
    for k in results:
        exp_dict = results[k]
        if 'line' in exp_dict:
            total += 1
        else:
            total += len(exp_dict['lines'])
    return total

# Credit to https://stackoverflow.com/questions/24575869/read-file-and-plot-cdf-in-python/37254481#37254481
def get_ecdf(data):
    # data_size=len(data)

    # # Set bins edges
    # data_set=sorted(set(data))
    # bins=np.append(data_set, data_set[-1]+1)

    # # Use the histogram function to bin the data
    # counts, bin_edges = np.histogram(data, bins=bins, density=False)

    # counts=counts.astype(float)/data_size

    # # Find the cdf
    # cdf = np.cumsum(counts)

    # xret = bin_edges[0:-1]
    # yret = cdf

    # xret = np.insert(xret, 0, xret[0])
    # yret = np.insert(yret, 0, 0.)
    # # return (bin_edges[0:-1], cdf)
    # return (xret, yret)

    xret = np.sort(data)
    yret = np.arange(len(data)) / float(len(data))
    return (xret, yret)


# Credit to https://stackoverflow.com/questions/33345780/empirical-cdf-in-python-similiar-to-matlabs-one
def get_cdf(data):
    # convert sample to a numpy array, if it isn't already
    data = np.atleast_1d(data)

    # find the unique values and their corresponding counts
    quantiles, counts = np.unique(data, return_counts=True)

    # take the cumulative sum of the counts and divide by the data size to
    # get the cumulative probabilities between 0 and 1
    cumprob = np.cumsum(counts).astype(np.double) / data.size

    return quantiles, cumprob


def plot_results(results, out_fname, title="Graph title",
                 xlabel="X Label", ylabel="Y Label",
                 xlim=None, ylim=None,
                 cmap_n='tab20',
                 markers=['o','v','^','s','x','d','p','h','*', '>'],
                 markersize=3.5,
                 linewidth=1.5,
                 legend_title="Legend title",
                 plot_order=None,
                 drawstyle=None):
    cmap = get_cmap(get_num_lines(results), name=cmap_n)
    fig = plt.figure()
    ax = fig.add_subplot(111)

    # ax.set_xlim(0, int(results[results.keys()[0]][-1]['max_bw']))
    if xlim:
        ax.set_xlim(xlim[0], xlim[1])
    if ylim:
        ax.set_ylim(ylim[0], ylim[1])
    ax.grid(True, linestyle='--', linewidth=1)
    num_labels = 0
    results_list = plot_order if plot_order else results.keys()
    extra_xlines = []
    extra_ylines = []
    for exp in results_list:
        exp_dict = results[exp]
        lines = [exp_dict['line']] if 'line' in exp_dict else exp_dict['lines']
        labels = [exp_dict['label']] if 'label' in exp_dict else exp_dict['labels']
        to_plot = zip(lines, labels)
        for line, label in to_plot:
            x_vals, y_vals = line
            marker = markers[0] if markers else '.'
            color = exp_dict['color'] if 'color' in exp_dict else cmap[0]
            if 'color' not in exp_dict:
                cmap.pop(0)
            nline, = ax.plot(x_vals, y_vals, '-',
                             marker=marker, markersize=markersize, linewidth=linewidth,
                             color=color, label=label,
                             drawstyle=drawstyle)
            num_labels += 1
            if markers:
                markers.pop(0)
            lines.append(nline)
        if 'horiz_vals' in exp_dict:
            for entry in exp_dict['horiz_vals']:
                extra_xlines.append(entry)
        if 'vert_vals' in exp_dict:
            for entry in exp_dict['vert_vals']:
                extra_ylines.append(entry)
    for entry in extra_xlines:
        e_val = entry['value']
        e_color = entry['color']
        e_label = entry['label'] if 'label' in entry else None
        ax.axhline(y=e_val, linestyle='--', marker='.', markersize=0.0, linewidth=1.2, color=e_color, label=e_label)
        if e_label:
            num_labels += 1
    for entry in extra_ylines:
        e_val = entry['value']
        e_color = entry['color']
        e_label = entry['label'] if 'label' in entry else None
        ax.axvline(x=e_val, linestyle='--', marker='.', markersize=0.0, linewidth=1.2, color=e_color, label=e_label)
        if e_label:
            num_labels += 1
    ax.set_title(title, y=1.05)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)

    handles, labels = ax.get_legend_handles_labels()
    # sort both labels and handles by labels
    # labels, handles = zip(*sorted(zip(labels, handles), key=lambda t: int(t[0])))
    if legend_title:
        lgd = plt.legend(handles, labels, bbox_to_anchor=(0.5, -0.125), loc='upper center', borderaxespad=0.5, ncol=num_labels,
                         title=legend_title)
        plt.savefig(out_fname, bbox_extra_artists=(lgd,), bbox_inches='tight')
    else:
        plt.savefig(out_fname, bbox_inches='tight')

def plot_results_box(results, out_fname, title="Graph title",
                     xlabel="X Label", ylabel="Y Label",
                     ylim=None,
                     xticks=None):
    fig = plt.figure()
    ax = fig.add_subplot(111)
    ax.grid(True, linestyle='--', linewidth=1)

    if ylim:
        ax.set_ylim(ylim[0], ylim[1])

    ax.boxplot(results, vert=True, sym='', whis='range',
               labels=xticks if xticks else [i for i in range(1, len(results)+1)])

    ax.set_title(title, y=1.05)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)

    plt.savefig(out_fname, bbox_inches='tight')
