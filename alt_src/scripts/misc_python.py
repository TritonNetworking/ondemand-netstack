import numpy as np
from . import graph_utils

def read_data_file(filename):
    with open(filename, "r") as f:
        s = f.read()
    s = s.strip()
    vals = []
    for v in s.split():
        try:
            vals.append(int(v))
        except ValueError:
            continue
    return vals

def printstats(filename):
    vals = read_data_file(filename)
    print("Mean:\t%.3f" % np.mean(vals))
    print("Stddev:\t%.3f" % np.std(vals))

def countbig(filename, big=150):
    vals = read_data_file(filename)
    big_vals = 0
    for v in vals:
        if v >= big:
            big_vals +=1

    return big_vals

def plot_cdf_set_all(exps,
                     out_fname,
                     title="Graph title",
                     xlabel="X Label", ylabel="Y Label",
                     xlim=None, ylim=None,
                     cmap_n="tab10"):
    cdfs = {}
    for expname in exps:
        exp = exps[expname]
        fname = exp["filename"]
        vals = read_data_file(fname)

        ex, ey = graph_utils.get_cdf(vals)
        cdfs[expname] = {"label": expname,
                         "line": (ex, ey)}

    graph_utils.plot_results(cdfs, out_fname,
                             title=title,
                             xlabel=xlabel,
                             ylabel=ylabel,
                             xlim=xlim,
                             ylim=ylim,
                             cmap_n=cmap_n,
                             markers=None,
                             markersize=0.0,
                             legend_title="Config",
                             drawstyle='steps-post')

import yaml
def plot_ethtool_delta(yaml_before, yaml_after):
    with open(yaml_before, "r") as f:
        before = yaml.load(f)["NIC statistics"]
    with open(yaml_after, "r") as f:
        after = yaml.load(f)["NIC statistics"]

    for k in before:
        try:
            b = int(before[k])
            a = int(after[k])
            if a - b != 0:
                print("%s: %d" % (k, a-b))
        except:
            continue

