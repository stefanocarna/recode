from email.policy import default
import os
import json
from color_printer import *
import numpy as np
import string
import random
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import matplotlib.colors as mcolors
from shell_cmd import * 

import sys
sys.path.append("...")
import utils.base.app as app

PLUGIN_NAME = "scheduler"
HELP_DESC = "[cschedr] Access and manipulate collected data"


class GroupProfile:
    def __init__(self, id, name, set, samples, workers):
        self.id = id
        self.name = name
        self.set = set
        self.samples = samples
        self.workers = workers
        self.metrics = {}
        self.totTime = 0
        self.cpuTime = 0
        self.procTime = 0
        self.nMetrics = []
        self.cMetrics = []
        self.powerTime = 1
        self.powerUnits = [1, 1, 1]
        self.powers = [1, 1, 1, 1, 1]

    def getOccupancy(self):
        return (self.procTime * 100) / self.totTime

    def getSet(self, groupProfiles):

        if len(self.set) == 0:
            return []

        set = []
        for s in self.set.split(" "):
            set.append(groupProfiles[s][0].name)

        return set

    def addMetric(self, name, values):
        self.metrics[name] = values
        self.nMetrics.append(name)

        cValues = 0
        for i, m in enumerate(values):
            cValues += m * i

        cValues /= int(self.samples) + 1
        cValues *= 100 / len(values)
        self.cMetrics.append(cValues)


class CSchedProfile:
    def __init__(self, parts, score, retire, energy, occupancy):
        self.parts = parts
        self.score = score
        self.retire = retire
        self.energy = energy
        self.occupancy = occupancy
        self.name = OS_ID


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    plug_parser.add_argument(
        "-p",
        "--plot",
        action="store_true",
        required=False,
        help="Plot groups data",
    )

    plug_parser.add_argument(
        "-pc",
        "--plot-compress",
        action="store_true",
        required=False,
        help="Plot groups (compressed) data ",
    )

    plug_parser.add_argument(
        "-d",
        "--dir",
        metavar="F",
        nargs='?',
        type=str,
        default="",
        help="Set dir where to find files",
    )

    plug_parser.add_argument(
        "-n",
        "--name",
        nargs='?',
        type=str,
        help="Set name used to label saved data"
    )


def autolabel(rects, ax, prec=1):
    """Attach a text label above each bar in *rects*, displaying its height."""
    for rect in rects:
        height = rect.get_height()
        ax.annotate(
            ("{:." + str(prec) + "f}").format(height),
            xy=(rect.get_x() + rect.get_width() / 2, height),
            xytext=(0, 3),  # 3 points vertical offset
            textcoords="offset points",
            ha="center",
            va="bottom",
        )


def plot_histo(profile, ax):
    plt.figure(figsize=[10, 5])
    plt.title(
        "Group: " + profile.name + " - Set: " + str(profile.getSet(groupProfiles)),
        fontsize=15,
    )
    plt.xlim(-1, 11)
    plt.grid(axis="y", alpha=0.75)
    plt.xlabel("Metric", fontsize=15)
    plt.ylabel("Percentage", fontsize=15)
    plt.xticks(fontsize=15)
    plt.yticks(fontsize=15)

    x = np.arange(10)

    width = 0.08

    for i, m in enumerate(profile.nMetrics):
        plt.bar(x[i] + (i * width), profile.metrics[m], width=width, alpha=0.7)
        plt.xticks(x, list(map(lambda x: str((x * 10)) + "%", x)))

    plt.legend(profile.nMetrics)

    plt.show()


def plot_histo_compress(profile, ax):

    ax.set_title(
        "SCHED: "
        + str(profile.getSet(groupProfiles))
        + " #S:"
        + profile.samples
        + " #W: "
        + profile.workers,
        fontsize=10,
    )

    ax.grid(axis="y", alpha=0.75)

    width = 0.5

    metrics = profile.cMetrics + [
        (profile.cpuTime * 100) / profile.totTime,
        (profile.procTime * 100) / profile.totTime,
    ]

    metrics = metrics + [profile.powers[0], profile.powers[1], profile.powers[2]]

    x = np.arange(len(metrics))
    ax.set_xlim([-1, len(metrics)])
    ax.set_ylim([0, 100])

    bars = ax.bar(
        x,
        metrics,
        width=width,
        alpha=0.7,
        # color=[
        #     "black",
        #     "red",
        #     "green",
        #     "purple",
        #     "blue",
        #     "cyan",
        #     "darkgray",
        #     "pink",
        #     "orange",
        #     "gray",
        # ],
    )

    autolabel(bars, ax)

    labels = profile.nMetrics + ["Tcpu", "Tproc"]
    labels = labels + ["ePKG", "eCORE", "eDRAM"]

    ax.set_xticks(x)
    ax.set_xticklabels(labels)

    # L1
    ax.axvline(3.5, linewidth=1, ls=":", color="b")
    # L2
    ax.axvline(5.5, linewidth=1, ls=":", color="b")
    # CPU
    ax.axvline(9.5, linewidth=1, ls="-.", color="gray")
    # ENERGY
    ax.axvline(11.5, linewidth=1, ls="-.", color="gray")


def plot_profile_histo(groupProfiles, maxPower, compress=False):

    firstList = groupProfiles[list(groupProfiles.keys())[0]]

    if len(groupProfiles) > 1:
        fig, axs = plt.subplots(len(firstList), len(groupProfiles), squeeze=False)
    else:
        pr_err("\n *** Cannot plot less than 2 figures ***\n")
        return

    fig.set_size_inches(18.5, 10.5)

    # fig.suptitle('GROUP ID: ' + id, fontsize=14)

    plt.xticks(fontsize=10)
    plt.yticks(fontsize=10)

    for i, k in enumerate(groupProfiles.keys()):
        for j, p in enumerate(groupProfiles[k]):
            if compress:
                p.powers[0] = (100 * p.powers[0]) / maxPower
                p.powers[1] = (100 * p.powers[1]) / maxPower
                p.powers[4] = (100 * p.powers[4]) / maxPower
                print(j, i)
                plot_histo_compress(p, axs[j][i])
            else:
                plot_histo(p, axs[j][i])

    """ Creates a fake subtitle for each column """
    grid = plt.GridSpec(1, len(groupProfiles))
    for i, k in enumerate(list(groupProfiles.keys())):
        fake = fig.add_subplot(grid[i])
        #  '\n' is important
        fake.set_title(groupProfiles[k][0].name + "\n", fontweight="semibold", size=15)
        fake.set_axis_off()

    for ax in axs.flat:
        ax.set(xlabel="Metric", ylabel="Percentage")
        ax.label_outer()

    plt.show()


def simple_plot(ax, x, values, max=0, reverse=False):
    width = 0.5

    if (reverse):
        colors = ["limegreen", "limegreen", "green", "green", "green", "blue", "red"]
    else:
        colors = ["red", "orange", "limegreen", "green"]

    cmap = mcolors.LinearSegmentedColormap.from_list("", colors)


    df = pd.DataFrame(values)

    if max == 0:
        color = cmap(df.values / df.values.max())
    else:
        color = cmap(df.values / max)
    

    return ax.bar(
        x,
        values,
        width=width,
        alpha=0.8,
        color=color
        # color=[
        #     "black",
        #     "red",
        #     "green",
        #     "purple",
        #     "blue",
        #     "cyan",
        #     "darkgray",
        #     "pink",
        #     "orange",
        #     "gray",
        # ],
    )


#!/usr/bin/env python
from matplotlib.ticker import NullFormatter  # useful for `logit` scale
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
import PySimpleGUI as sg
import matplotlib

matplotlib.use("TkAgg")


def draw_figure(canvas, figure):
    figure_canvas_agg = FigureCanvasTkAgg(figure, canvas)
    figure_canvas_agg.draw()
    figure_canvas_agg.get_tk_widget().pack(side="top", fill="both", expand=1)
    return figure_canvas_agg


def plot_csched_histo(path=None):
    global cSchedProfiles
    
    sns.set_style("white")
    # sns.color_palette("Paired")
    sns.set_color_codes("pastel")

    fig, axs = plt.subplots(2, 2, squeeze=False)

    fig.set_size_inches(18.5, 10.5)

    plt.xticks(fontsize=10)
    plt.yticks(fontsize=10)

    labels = []
    score = []
    energy = []
    retire = []
    occupancy = []

    print("csched len", len(cSchedProfiles), len(enum_chars))

    for cs in cSchedProfiles:
        cs.energy *= energyUnit
        cs.retire /= 100
        cs.occupancy /= 100
        cs.score = cs.energy / (cs.occupancy * cs.retire)

    csOS = cSchedProfiles.pop(0)

    cSchedProfiles.sort(key=lambda x: x.score, reverse=True)

    cSchedProfiles = cSchedProfiles[:8] + cSchedProfiles[-8:]
    random.shuffle(cSchedProfiles)
    cSchedProfiles.insert(0, csOS)

    energy = [cs.energy for cs in cSchedProfiles]
    retire = [cs.retire * 100 for cs in cSchedProfiles]
    occupancy = [cs.occupancy * 100 for cs in cSchedProfiles]
    score = [cs.score for cs in cSchedProfiles]
    
    # for i, e in enumerate(cSchedProfiles):
    #     # labels.append(enum_chars[i])
    #     energy.append(energyUnit * e.energy)
    #     retire.append(e.retire / 100)
    #     occupancy.append(e.occupancy / 100)
    #     # score.append(energy[-1] / (retire[-1] * occupancy[-1]))
    #     score.append(energy[-1] / (occupancy[-1] * retire[-1]))

    # if (len(cSchedProfiles) > 16):
    #     """ Get some max and some min """
    #     retire = [x for _, x in sorted(zip(score, retire), reverse=True)]
    #     occupancy = [x for _, x in sorted(zip(score, occupancy), reverse=True)]
    #     energy = [x for _, x in sorted(zip(score, energy), reverse=True)]
    #     sorted(score, reverse=True)

    #     retire = retire[:16] + retire[-16:]
    #     occupancy = occupancy[:16] + occupancy[-16:]
    #     energy = energy[:16] + energy[-16:]
    #     score = score[:16] + score[-16:]

    labels = getCSNameList(cSchedProfiles)
    print("lables", labels)

    axs[0][0].set_title("SCORE : ENERGY / (OCCUPANCY * USEFUL WORK)", fontsize=11, fontweight="bold")
    axs[0][1].set_title("ENERGY : Joule", fontsize=11, fontweight="bold")
    axs[1][0].set_title("USEFUL WORK : % TOTAL CPU CLOCKS", fontsize=11, fontweight="bold")
    axs[1][1].set_title("OCCUPANCY : % TOTAL CPU TIME", fontsize=11, fontweight="bold")
    for ax in axs.flat:
        ax.grid(axis="y", alpha=0.75)
        ax.set_xlim([-1, len(labels)])

    bars = simple_plot(axs[0][0], labels, score, 0, True)
    axs[0][0].set_ylim([0, max(score) * 1.15])
    axs[0][0].tick_params(axis='both', labelsize=12)
    # autolabel(bars, axs[0][0])

    bars = simple_plot(axs[0][1], labels, energy, 0, True)
    axs[0][1].set_ylim([0, max(energy) * 1.15])
    axs[0][1].tick_params(axis='both', labelsize=12)
    # autolabel(bars, axs[0][1])

    bars = simple_plot(axs[1][0], labels, retire, 1)
    axs[1][0].set_ylim([0, 100])
    axs[1][0].tick_params(axis='both', labelsize=12)
    # autolabel(bars, axs[1][0], 2)

    bars = simple_plot(axs[1][1], labels, occupancy, 1)
    axs[1][1].set_ylim([0, 100])
    axs[1][1].tick_params(axis='both', labelsize=12)
    # autolabel(bars, axs[1][1], 2)

    ax.set_xticklabels(labels)

    print("cSchedProfiles", [cs.name for cs in cSchedProfiles])

    if path is not None:
        name = readProcName(path)[0].strip()
        plt.savefig('cosched' + name + '.pdf')
        plot_csched_info(cSchedProfiles, groupProfiles, name)

    """ RESTORE """
    # # define the window layout
    # layout = [[sg.Text("CoScheduling data")], [sg.Canvas(key="-CANVAS-")], [sg.Button("Legend", key="showLegend")]]

    # # create the form and show it without the plot
    # window = sg.Window(
    #     "CoScheduling data",
    #     layout,
    #     resizable=True,
    #     finalize=True,
    #     element_justification="center",
    #     font="Helvetica 18",
    # )

    # fig_canvas_agg = draw_figure(window["-CANVAS-"].TKCanvas, fig)

    # while True:
    #     event, values = window.read()
    #     if event == "Exit" or event == sg.WIN_CLOSED:
    #         break
    #     if event == "showLegend":
    #         plot_csched_info(cSchedProfiles, groupProfiles)
        
    # window.close()


def __tm(m):
    if m == "l0_bb":
        return "BB"
    elif m == "l0_bs":
        return "BS"
    elif m == "l0_re":
        return "RE"
    elif m == "l0_fb":
        return "FB"
    elif m == "l1_mb":
        return "1MB"
    elif m == "l1_cb":
        return "1CB"
    elif m == "l2_l1b":
        return "L1"
    elif m == "l2_l2b":
        return "L2"
    elif m == "l2_l3b":
        return "L3"
    elif m == "l2_dramb":
        return "MEM"
    else:
        return m


def readProcName(path):
    # return RAW
    file = open(path + "/name", "r")
    data = file.readlines()
    file.close()
    return data


def readProcGroup(path):
    # return RAW
    file = open(path + "/groups", "r")
    data = file.readlines()
    file.close()
    return data


def readProcCSched(path):
    # return RAW
    file = open(path + "/csched", "r")
    data = file.readlines()
    file.close()
    return data


groupProfiles = {}
energyUnit = 0.5


def parseGroupProfiles(path):
    global energyUnit
    onMetrics = False
    profile = None

    for line in readProcGroup(path):
        # print("Parsing: " + line)
        if "ID" in line:
            # Add the new profile
            if profile is not None:
                if profile.id not in groupProfiles:
                    groupProfiles[profile.id] = []
                groupProfiles[profile.id].append(profile)

            id = line.replace("ID", "").strip()
            onMetrics = False
        elif "NAME" in line:
            name = line.replace("NAME", "").strip()
            onMetrics = False
        elif "SET" in line:
            set = line.replace("SET", "").strip()
            onMetrics = False
        elif "SAMPLES" in line:
            samples = line.replace("SAMPLES", "").strip()
            onMetrics = False
        elif "TASKS" in line:
            workers = line.replace("TASKS", "").strip()
            onMetrics = False
        elif "TOT_TIME" in line:
            totTime = int(line.replace("TOT_TIME", "").strip())
            onMetrics = False
        elif "CPU_TIME" in line:
            cpuTime = int(line.replace("CPU_TIME", "").strip())
            onMetrics = False
        elif "PROC_TIME" in line:
            procTime = int(line.replace("PROC_TIME", "").strip())
            onMetrics = False
        elif "POWER_TIME" in line:
            powerTime = int(line.replace("POWER_TIME", "").strip())
            onMetrics = False
        elif "POWER_UNITS" in line:
            rawPowerUnits = line.replace("POWER_UNITS", "").strip().split(" ")
            powerUnits = []
            for u in rawPowerUnits:
                powerUnits.append(0.5 ** (float(u)))
            energyUnit = powerUnits[1]
            onMetrics = False
        elif "POWERS" in line:
            rawPowers = line.replace("POWERS", "").strip().split(" ")
            powers = []
            for u in rawPowers:
                powers.append(float(u) * powerUnits[1])
            onMetrics = False
        elif "METRICS" in line:
            onMetrics = True
            profile = GroupProfile(id, name, set, samples, workers)
            profile.totTime = totTime
            profile.cpuTime = cpuTime
            profile.procTime = procTime
            profile.powerTime = powerTime
            profile.powerUnits = powerUnits
            profile.powers = powers

        elif onMetrics:
            elems = line.split(" ")
            metric = __tm(elems[0])
            values = list(map(lambda x: int(x), elems[1:]))
            profile.addMetric(metric, values)
        else:
            print("Unknown token: " + line)

    if profile is not None:
        if profile.id not in groupProfiles:
            groupProfiles[profile.id] = []
        groupProfiles[profile.id].append(profile)


cSchedProfiles = []
enum_chars = []


def getCSName(csList):
    num = len(csList) - 1
    off = len(string.ascii_uppercase)
    label = ""

    if num == 0:
        return OS_ID

    while (num >= off):
        tmp = num / off
        label += string.ascii_uppercase[int(tmp)]
        num = int(num / off) + (num % off) - 1

    label += string.ascii_uppercase[num - 1]
    return label


def getCSNameList(csList):
    names = []
    for cs in csList:
        names.append(cs.name)
    return names


def parseCSchedProfiles(path):
    parts = []
    onParse = False

    for line in readProcCSched(path):
        if "PARTS" in line:
            if onParse:
                cSchedProfiles.append(
                    CSchedProfile(parts, score, retire, energy, occupancy)
                )
                cSchedProfiles[-1].name = getCSName(cSchedProfiles)
            parts = []
            onParse = True
        elif "SCORE" in line:
            score = int(line.replace("SCORE", "").strip())
        elif "ENERGY" in line:
            energy = int(line.replace("ENERGY", "").strip())
        elif "RETIRE" in line:
            retire = int(line.replace("RETIRE", "").strip())
        elif "OCCUPANCY" in line:
            occupancy = int(line.replace("OCCUPANCY", "").strip())
        else:
            parts.append(line.strip().split(" "))

    if onParse:
        cSchedProfiles.append(CSchedProfile(parts, score, retire, energy, occupancy))
        cSchedProfiles[-1].name = getCSName(cSchedProfiles)


    # enum_chars.append("**")

    # if (len(cSchedProfiles) > len(string.ascii_uppercase)):
    #     for c in string.ascii_uppercase:
    #         enum_chars.append("x" + c)
    #         enum_chars.append("y" + c)
    #         enum_chars.append("w" + c)
    #         enum_chars.append("z" + c)
    #     else:
    #         for c in string.ascii_uppercase:
    #             enum_chars.append(c)

    # for p in cSchedProfiles:
    #     print(p.parts)


class CSched:
    def __init__(self, id, parts):
        self.id = id
        self.parts = parts

    def addPart(self, part, val):
        self.parts.appen(tuple([part, val]))

    def getPartName(self, pId, groups):
        names = []
        for g in self.parts[pId]:
            names.append(groups[g][0].name)

        return names

    def getPartNameIdThd(self, pId, groups, ghash):
        names = []
        for g in self.parts[pId]:
            # names.append(ghash[groups[g][0].id] + " (" + groups[g][0].name + ":" + groups[g][0].workers + ")")
            names.append(groups[g][0].name + " (" + ghash[groups[g][0].id] + ")")

        return names

    def getPartCodeIdThd(self, pId, groups, ghash):
        names = []
        for g in self.parts[pId]:
            # names.append(ghash[groups[g][0].id] + " (" + groups[g][0].name + ":" + groups[g][0].workers + ")")
            names.append("(" + ghash[groups[g][0].id] + ")")

        return names

    def getPartWeight(self, pId, groups):
        weight = 0
        for g in self.parts[pId]:
            weight += groups[g][0].getOccupancy()

        return round(weight, 2)


OS_ID = "OS"


def plot_csched_info(csched, groups, name):

    legend = {}
    data = {}

    ghash = {}

    enum_chars = getCSNameList(cSchedProfiles)

    # Create fake hash
    for i, g in enumerate(groups):
        ghash[groups[g][0].id] = str(i)

    for i, e in enumerate(csched):
        legend[enum_chars[i]] = CSched(enum_chars[i], e.parts)

        if legend[enum_chars[i]].id == OS_ID:
            for k in range(len(e.parts)):
                part = legend[enum_chars[i]]

                if str(part.getPartNameIdThd(k, groups, ghash)) not in data.keys():
                    data[str(part.getPartNameIdThd(k, groups, ghash))] = [np.nan] * len(csched)
        
                data[str(part.getPartNameIdThd(k, groups, ghash))][i] = part.getPartWeight(k, groups)
        else:
            for k in range(len(e.parts)):
                part = legend[enum_chars[i]]

                if str(part.getPartCodeIdThd(k, groups, ghash)) not in data.keys():
                    data[str(part.getPartCodeIdThd(k, groups, ghash))] = [np.nan] * len(csched)
        
                data[str(part.getPartCodeIdThd(k, groups, ghash))][i] = part.getPartWeight(k, groups)
       
        # nr_parts = len(legend[enum_chars[i]].parts)
            # print(data[str(part.getPartNameIdThd(k, groups))])
            
            # print(part.getPartNameIdThd(k, groups), part.getPartWeight(k, groups))

    #         for g in p:
    #             legend[groupProfiles[g][0].id + ":" + str(groupProfiles[g][0].getOccupancy())]

    # print(legend)

    index = []
    index[:0] = enum_chars[:len(csched)]

    df = pd.DataFrame(
        data,
        index=index,
    )

    # plot dataframe
    ax = df.plot.barh(
        legend=False,
        figsize=(18.5, 10.5),
        stacked=True,
        width=0.9,
        color=["b", "r", "g", "orange"]
    )

    ax.tick_params(axis='both', labelsize=12)
    ax.set_title("Available CoScheduling with partitions", fontsize="12", fontweight="bold")

    labels = []
    for j in df.columns:
        for i in df.index:
            label = str(j) + ": " + str(df.loc[i][j])
            labels.append(label)    

    patches = ax.patches

    for label, rect in zip(labels, patches):
        width = rect.get_width()
        if width > 0:
            x = rect.get_x()
            y = rect.get_y()
            height = rect.get_height()

            names = label.split("]:")[0].replace("[", "").replace("\'", "").replace(",", " --")
            weight = label.split("]:")[1]
            ax.text(x + width / 2.0, (y + height / 2.0) + 0.2, names, ha="center", va="center")
            ax.text(x + width / 2.0, (y + height / 2.0) - 0.2, weight, ha="center", va="center", fontsize=10, fontweight='bold')

    fig = ax.get_figure()
    fig.set_size_inches(18.5, 10.5)

    plt.savefig('cosched_labels' + name + '.pdf')

    """ RESTORE """
    # layout = [[sg.Text("CoScheduling details")], [sg.Canvas(key="-CANVAS-")], [sg.Button("Close")]]

    # # create the form and show it without the plot
    # window = sg.Window(
    #     "CoScheduling info",
    #     layout,
    #     resizable=True,
    #     finalize=True,
    #     element_justification="center",
    #     font="Helvetica 14",
    # )

    # # add the plot to the window
    # fig_canvas_agg = draw_figure(window["-CANVAS-"].TKCanvas, fig)
    # event, values = window.read()
    # window.close()


def action_plot(path, csched=False, compress=False):
    global cSchedProfiles
    parseCSchedProfiles(path)
    parseGroupProfiles(path)

    if csched:
        plot_csched_histo(path)
    else:
        power_labels = ["PKG", "PP0", "PP1", "REST", "DRAM"]

        maxPower = 0
        for k in groupProfiles:
            for p in groupProfiles[k]:
                print("Profile: " + p.id)
                print("Power Time:")
                print("\t * {} CLKs".format(p.powerTime))
                print("Powers:")
                if p.powers[0] > maxPower:
                    maxPower = p.powers[0]
                for i, e in enumerate(p.powers):
                    print("\t* " + power_labels[i] + ": {:.6f} J".format(e))

        plot_profile_histo(groupProfiles, maxPower, compress)
        # for p in groupProfiles:
        #     profile = groupProfiles[p]
        #     print(groupProfiles)
        #     if (compress):
        #         plot_histo_compress(profile)
        #     else:
        #         plot_histo(profile)
        #     print(profile.metrics[metric])


def isInt(s):
    try: 
        int(s)
        return True
    except ValueError:
        return False

        
def store_data(name):

    path = "/home/userx/benchmark/recode_sched_logs"

    max = 0
    for d in os.listdir(path):
        print(max, d)
        if isInt(d) and max <= int(d):
            max = int(d) + 1

    path += "/" + str(max) + "/"

    cmd(["mkdir", path])

    cmd("cat /proc/recode/groups > " + path + "groups", sh=True)
    cmd("cat /proc/recode/csched > " + path + "csched", sh=True)
    if name:
        cmd("echo " + name + " > " + path + "name", sh=True)


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args):
    if not validate_args(args):
        return False

    print(args)
    if (args.dir == ""):
        args.dir = app.globalConf.readPath("recode_proc")

    if args.plot is not None and args.plot:
        action_plot(args.dir, False, False)

    if args.plot_compress is not None and args.plot_compress:
        action_plot(args.dir, True, True)

    store_data(args.name)

    return True
