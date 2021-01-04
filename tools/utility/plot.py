import matplotlib.pyplot as plt

# import numpy as np
# import os
import pandas as pd
import seaborn as sns

# import sys

from utility.decoder import EVENT_DECODER
import utility.proc_parser as parser

GENERAL = 6


def getDFfromData(data, idx):
    _df = pd.DataFrame(data[idx])
    _df["CPU"] = idx
    return _df


def meanDF(df, col, precision):
    idx = 0
    data = {"index": [], col: []}

    while idx < (len(df) - precision):
        mean = 0
        for i in range(precision):
            mean += df[idx + i]
        data[col].append(mean / precision)
        data["index"].append(idx)
        idx += precision

    return pd.DataFrame.from_dict(data).set_index("index")


def plotCpusData(cpus=[0]):

    data = parser.read_data_dict()

    #
    # [CPU0: {TSC: [], FX0: []....}]
    # [CPU1: {TSC: [], FX0: []....}]
    # ...
    # [CPUn: {TSC: [], FX0: []....}]
    #

    # Modify this to be consistent with all the datasets

    # if cpus is None or len(cpus) != 1:
    #     print("Alpha release: we support only one cpu plot at the moment")
    #     return

    # idx = cpus[0]

    idx = 0

    # Get the first not null dataset
    while (
        (len(cpus) == 0 or idx in cpus)
        and idx < len(data)
        and len(data[idx].keys()) < 1
    ):
        idx += 1

    # extract and update labels
    labels = list(data[idx].keys())
    labels.insert(0, "CPU")

    df = getDFfromData(data, idx)
    idx += 1

    while (len(cpus) == 0 or idx in cpus) and idx < len(data):
        df = df.append(getDFfromData(data, idx), ignore_index=True)
        idx += 1

    df = df.sort_values(by=["TSC"])
    df = df.reset_index(drop=True)

    print(df)

    df["TSC"] = df["TSC"] / df["TSC"][0]

    ndf = pd.DataFrame()

    M1 = EVENT_DECODER[labels[GENERAL + 1]] + "/" + EVENT_DECODER[labels[GENERAL]]

    M0 = "L1"
    M1 = "L2"
    P0 = "L2/L1"
    P1 = "L3/L1"
    P2 = "L2wb/L2in"
    P3 = "TLB2/L1"

    metrics = [P0, P1, P2, P3]

    ndf["TSC"] = df["TSC"]
    ndf["PID"] = df["PID"]
    ndf[M0] = df[labels[GENERAL]]
    ndf[M1] = df[labels[GENERAL + 1]]
    ndf[P0] = df[labels[GENERAL + 1]] / (df[labels[GENERAL]] + 1)
    ndf[P1] = df[labels[GENERAL + 2]] / (df[labels[GENERAL]] + 1)
    ndf[P2] = df[labels[GENERAL + 3]] / (df[labels[GENERAL + 4]] + 1)
    ndf[P3] = df[labels[GENERAL + 5]] / (df[labels[GENERAL]] + 1)

    # ndf = ndf.truncate(before=1)

    sns.set_theme(style="darkgrid")

    colors = [
        "green",
        "blue",
        "orange",
        "purple",
        "acqua",
        "grey",
        "pink",
        "brown",
    ]

    fig, axs = plt.subplots(len(metrics))

    mean = 128
    cp = 0
    for m in metrics:
        subplt = axs[cp]

        subplt.plot(
            ndf.index, ndf[m], "tab:" + colors[cp % len(colors)], label=m, marker="."
        )

        subplt.plot(
            ndf.index,
            ndf[m].rolling(window=mean, center=True).mean(),
            "tab:red",
            label="MEAN(" + str(mean) + ")",
            linestyle="-",
        )

        subplt.legend(loc="best")
        subplt.set_ylim([-0.1, 1.1])  # [ set_ylim ([- 1,]  0])
        cp += 1

    fig.suptitle("Side-Channel Detection metrics")

    plt.show()
