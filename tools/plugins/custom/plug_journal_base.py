from cmath import inf
from os import chdir, cpu_count
from os.path import isfile
import time
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import glob
import math
from utils.base import cmd
from utils.base import app

PLUGIN_NAME = "jbase"
HELP_DESC = "Journal base experiments"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    plug_parser.add_argument(
        "-fn",
        "--file_name",
        nargs=2,
        type=str,
        help="Name of the output file",
    )

    plug_parser.add_argument(
        "-f",
        "--frequency",
        nargs=2,
        type=int,
        default=16,
        help="Set the profiling frequency",
    )

    plug_parser.add_argument(
        "-p",
        "--parse",
        nargs="+",
        type=str,
        help="Parse and plot the given File",
    )

    plug_parser.add_argument(
        "-d",
        "--directory",
        type=str,
        help="Set the directory where to find the plot data",
    )

    plug_parser.add_argument(
        "-c",
        "--calibrate",
        action="store_true",
        required=False,
        help="Run calibration for single and parallel version of the stressors",
    )

    plug_parser.add_argument(
        "-eb",
        "--exec-bench",
        nargs="+",
        type=str,
        help="Start the benchmark",
    )


# def getTimeFromStressOutput(output):
#     for line in output.split("\n"):
#         if "successful run completed in " in line:
#             return float(
#                 output.split("successful run completed in ")[1].split("s")[0]
#             )
#     return 0

workloads = {
    "cpu": "16041",
    "cache": "464",
    "memcpy": "4843",
    "atomic": "9838247",
    # "x86syscall": "20000",
    "matrix-3d": "8618",
    "bsearch": "4189",
}

SMT = 2 * cpu_count()

workloadsSmt = {
    "cpu": "38409",
    "cache": "2000",
    "memcpy": "8483",
    "atomic": "864008",
    # "x86syscall": "20000",
    "matrix-3d": "5583",
    "bsearch": "10457",
}

def openFile(name, rights):
    try:
        return open(name, rights)
    except IOError:
        print("cannot open " + name)
        return None


def getTimeFromStressOutput(output, type):
    for line in output.split("\n"):
        if type in line:
            raw = line.split("]")[1].split(type)[0].split("s")[0].strip()
            return float(raw)
    return 0


def getSamplesFromProc():
    output = cmd.cmd("cat /proc/recode/info", type=Pipe.OUT, sh=True)
    print(output)
    for line in output.split("\n"):
        if "TRACKED PMIs" in line:
            return int(line.split("TRACKED PMIs")[1])

    return 0


def getPMIsFromProc():
    output = cmd.cmd("cat /proc/pmudrv/info", type=Pipe.OUT, sh=True)
    for line in output.split("\n"):
        if "PMIs" in line:
            return int(line.split("PMIs")[1])

    return 0


AVG_TIME = [3, 4]
GOAL_TIME = sum(AVG_TIME) / len(AVG_TIME)


def calibrateWk(wk, nr, cOps, avg_time=AVG_TIME, goal_time=GOAL_TIME):
    _cmd_stress = ["stress-ng", "--times"]
    cOps = 100

    while True:
        wkExec = _cmd_stress + (
            "--" + wk + " " + str(nr) + " --" + wk + "-ops " + str(cOps)
        ).split(" ")
        out, err, ret = cmd.cmd(wkExec)

        runtimeT = 0
        for line in err.split("\n"):
            if "run completed in" in line:
                print(line)
                runtimeT = float(line.split("run completed in")[1].replace("s", ""))
        # runtimeT = getTimeFromStressOutput(err, "total time")

        if runtimeT == 0:
            cOps *= 10
            continue

        if runtimeT < avg_time[0] or runtimeT > avg_time[1]:
            cOps = int(cOps * (goal_time / runtimeT))
            print("Time " + str(runtimeT) + " - Set cOps to " + str(cOps))
        else:
            print(
                wk
                + " "
                + str(nr)
                + " CALIBRATED at: "
                + str(cOps)
                + " (Time "
                + str(runtimeT)
                + ")"
            )
            return cOps


def calibrate():
    _cmd_stress = ["stress-ng", "--times"]

    print("CALIBRATING PHASE")

    for wk in workloads:
        workloads[wk] = calibrateWk(wk, 1, 100)

    for wk in workloadsSmt:
        cOps = int(workloads[wk])
        workloadsSmt[wk] = calibrateWk(wk, SMT, cOps)


def jbase_4_0_0(args):

    fOut = openFile("results/jbase_4_0_0.txt", "w")
    if fOut is None:
        return

    _cmd_stress = ["stress-ng", "--times"]

    nRuns = 3

    print("jbase_4_0_0")

    fOut.write("MOD 1\n")

    for wk in workloads:
        runtimeUAvg = 0
        runtimeSAvg = 0
        runtimeTAvg = 0
        fOut.write("WK " + wk + "\n")
        fOut.write("OPS " + str(workloads[wk]) + "\n")
        for run in range(nRuns):
            wkExec = _cmd_stress + (
                "--" + wk + " 1 --" + wk + "-ops " + str(workloads[wk])
            ).split(" ")

            out, err, ret = cmd.cmd(wkExec)
            print(" ".join(wkExec))
            print("ERR:", err)

            runtimeU = getTimeFromStressOutput(err, "user time")
            runtimeS = getTimeFromStressOutput(err, "system time")
            runtimeT = getTimeFromStressOutput(err, "total time")

            runtimeUAvg += runtimeU
            runtimeSAvg += runtimeS
            runtimeTAvg += runtimeT

            fOut.write(
                "{:.3f}".format(runtimeU)
                + " "
                + "{:.3f}".format(runtimeS)
                + " "
                + "{:.3f}".format(runtimeT)
                + "\n"
            )

        runtimeUAvg /= nRuns
        runtimeSAvg /= nRuns
        runtimeTAvg /= nRuns

        fOut.write(
            "AVG\n"
            + "{:.3f}".format(runtimeUAvg)
            + " "
            + "{:.3f}".format(runtimeSAvg)
            + " "
            + "{:.3f}".format(runtimeTAvg)
            + "\n\n"
        )

    fOut.write("MOD " + str(SMT) + "\n")

    for wk in workloadsSmt:
        runtimeUAvg = 0
        runtimeSAvg = 0
        runtimeTAvg = 0
        fOut.write("WK " + wk + "\n")
        for run in range(nRuns):
            wkExec = _cmd_stress + (
                "--"
                + wk
                + " "
                + str(SMT)
                + " --"
                + wk
                + "-ops "
                + str(workloadsSmt[wk])
            ).split(" ")

            out, err, ret = cmd.cmd(wkExec)
            print(" ".join(wkExec))
            print("ERR:", err)

            runtimeU = getTimeFromStressOutput(err, "user time")
            runtimeS = getTimeFromStressOutput(err, "system time")
            runtimeT = getTimeFromStressOutput(err, "total time")

            runtimeUAvg += runtimeU
            runtimeSAvg += runtimeS
            runtimeTAvg += runtimeT

            fOut.write(
                "{:.3f}".format(runtimeU)
                + " "
                + "{:.3f}".format(runtimeS)
                + " "
                + "{:.3f}".format(runtimeT)
                + "\n"
            )

        runtimeUAvg /= nRuns
        runtimeSAvg /= nRuns
        runtimeTAvg /= nRuns

        fOut.write(
            "AVG\n"
            + "{:.3f}".format(runtimeUAvg)
            + " "
            + "{:.3f}".format(runtimeSAvg)
            + " "
            + "{:.3f}".format(runtimeTAvg)
            + "\n\n"
        )

        fOut.flush()

    fOut.close()


def jbase_4_1_1(args):
    wrapper = "wrapper"
    if not isfile(wrapper):
        # Compile Wrapper
        cmd.cmd("make")

    _cmd_wrapper = [app.globalConf.readPath("accessory") + "/" + wrapper]
    _cmd_stress = ["stress-ng", "--times"]

    frequencies = ["20", "16", "12"]

    vectors = {"NMI": "0", "IRQ": "1"}

    nRuns = 3

    for freq in frequencies:

        fOut = openFile("results/jbase_4_1_" + freq + ".txt", "w")
        if fOut is None:
            return

        for vec in vectors:

            cmd.cmd("python tools/recode.py module -m journal_base -c -l -u".split(" "))

            cmd.cmd("echo " + vectors[vec] + " > /proc/pmudrv/vector", sh=True)

            cmd.cmd(("python tools/recode.py config -f " + freq).split(" "))

            time.sleep(1)

            cmd.cmd("python tools/recode.py config -s system".split(" "))

            fOut.write("VECTOR " + vec + "\n\n")
            time.sleep(1)

            for wk in workloads:
                cpuPmisAvg = 0
                samplesAvg = 0
                runtimeUAvg = 0
                runtimeSAvg = 0
                runtimeTAvg = 0
                fOut.write("WK " + wk + " @ " + str(workloads[wk]) + "\n")
                for run in range(nRuns):
                    # Reset stats
                    if vec != "OFF":
                        cmd.cmd("echo 0 > /proc/recode/info", sh=True)
                        cmd.cmd("echo 0 > /proc/pmudrv/info", sh=True)

                        wkExec = (
                            _cmd_wrapper
                            + [wk]
                            + _cmd_stress
                            + (
                                "--" + wk + " 1 --" + wk + "-ops " + str(workloads[wk])
                            ).split(" ")
                        )

                    else:
                        wkExec = _cmd_stress + (
                            "--" + wk + " 1 --" + wk + "-ops " + str(workloads[wk])
                        ).split(" ")

                    out, err, ret = cmd.cmd(wkExec)

                    if vec != "OFF":
                        cpuPmis = getPMIsFromProc()
                        samples = getSamplesFromProc()
                    else:
                        cpuPmis = 0
                        samples = 0
                    # runtime = getTimeFromStressOutput(err)

                    runtimeU = getTimeFromStressOutput(err, "user time")
                    runtimeS = getTimeFromStressOutput(err, "system time")
                    runtimeT = getTimeFromStressOutput(err, "total time")

                    print(wkExec)
                    print(
                        wk
                        + " ("
                        + str(run)
                        + ") -> "
                        + str(ret)
                        + " : "
                        + str(runtimeT)
                        + " - "
                        + str(samples)
                    )

                    cpuPmisAvg += cpuPmis
                    samplesAvg += samples

                    runtimeUAvg += runtimeU
                    runtimeSAvg += runtimeS
                    runtimeTAvg += runtimeT

                    fOut.write(
                        "{:.3f}".format(runtimeU)
                        + " "
                        + "{:.3f}".format(runtimeS)
                        + " "
                        + "{:.3f}".format(runtimeT)
                        + " - "
                        + str(samples)
                        + " "
                        + str(cpuPmis)
                        + "\n"
                    )

                cpuPmisAvg /= nRuns
                samplesAvg /= nRuns
                runtimeUAvg /= nRuns
                runtimeSAvg /= nRuns
                runtimeTAvg /= nRuns

                fOut.write(
                    "AVG\n"
                    + "{:.3f}".format(runtimeUAvg)
                    + " "
                    + "{:.3f}".format(runtimeSAvg)
                    + " "
                    + "{:.3f}".format(runtimeTAvg)
                    + " - "
                    + "{:.3f}".format(samplesAvg)
                    + " "
                    + "{:.3f}".format(cpuPmisAvg)
                    + "\n\n"
                )

                fOut.flush()

            cmd.cmd("python tools/recode.py config -s off".split(" "))

        fOut.close()


def isInt(s):
    try:
        float(s)
        return True
    except ValueError:
        return False


def jbase_4_2_1(args):

    fOut = openFile("results/jbase_4_2_1.txt", "w")
    if fOut is None:
        return

    chdir("/home/userx/git/SHook")

    plugins = ["DUMMY", "KPROBE", "TRACEP"]

    _cmd_stress = "stress-ng --times"
    _cmd_stress_tt = []

    ctxWks = ["switch", "clone", "fork"]

    for wk in ctxWks:
        cOps = calibrateWk(wk, SMT, 100, [4, 5], 4.5)

        _cmd_stress_tt.append((
            _cmd_stress
            + " --"
            + wk
            + " "
            + str(SMT)
            + " --"
            + wk
            + "-ops "
            + str(cOps)).split(" ")
        )

    nRuns = 1

    for _cmd_stress in _cmd_stress_tt:

        fOut.write(" ".join(_cmd_stress) + "\n")

        for plug in plugins:
            cmd.cmd("make PLUGIN=" + plug, sh=True)
            cmd.cmd("make unload", sh=True)
            cmd.cmd("make load", sh=True)

            fOut.write("PLUG " + plug + "\n")

            runtimeUAvg = 0
            runtimeSAvg = 0
            runtimeTAvg = 0

            for run in range(nRuns):

                wkExec = _cmd_stress
                print("Executing:", " ".join(wkExec))
                out, err, ret = cmd.cmd(wkExec)

                print(err)

                runtimeU = getTimeFromStressOutput(err, "user time")
                runtimeS = getTimeFromStressOutput(err, "system time")
                        
                for line in err.split("\n"):
                    if "run completed in" in line:
                        print(line)
                        runtimeT = float(line.split("run completed in")[1].replace("s", ""))

                # runtimeT = getTimeFromStressOutput(err, "total time")

                runtimeUAvg += runtimeU
                runtimeSAvg += runtimeS
                runtimeTAvg += runtimeT

            runtimeUAvg /= nRuns
            runtimeSAvg /= nRuns
            runtimeTAvg /= nRuns

            fOut.write(
                "AVG\n"
                + "{:.3f}".format(runtimeUAvg)
                + " "
                + "{:.3f}".format(runtimeSAvg)
                + " "
                + "{:.3f}".format(runtimeTAvg)
                + "\n\n"
            )

            fOut.flush()

    fOut.close()


def exec_cmd(nRuns, fOut, cmdStress, tmpFile=False, perf=False):
    runtimeUAvg = 0
    runtimeSAvg = 0
    runtimeTAvg = 0
    perfSampAvg = 0

    for run in range(nRuns):
        wkExec = cmdStress
        print("EXEC:", " ".join(wkExec))

        if tmpFile:
            p = cmd.dcmd(" ".join(wkExec + ["2>", "tmpFile.txt"]), sh=True)
            p.wait()

            tmpF = open("tmpFile.txt", "r")
            err = "".join(tmpF.readlines())
            tmpF.close()
            cmd.cmd("rm tmpFile.txt", sh=True)
            out = ""
            ret = 0
        else:
            out, err, ret = cmd.cmd(wkExec)

        if ret:
            print("Error while exec: ", " ".join(wkExec))
            print("OUT", out)
            print("ERR", err)
            exit(-1)

        parse = False
        
        for line in err.split("\n"):
            if "run completed in" in line:
                print(line)
                runtimeT = float(line.split("run completed in")[1].replace("s", ""))
            # if ("(real time)" in line):
            #     parse = True
            # elif (parse):
            #     runtimeT = float(line.split("]")[1].strip().split()[2])
            #     break
       
        print("RUNETIME", runtimeT)
        print("ERR", err)
        runtimeU = getTimeFromStressOutput(err, "user time")
        runtimeS = getTimeFromStressOutput(err, "system time")
        # runtimeT = getTimeFromStressOutput(err, "total time")

        runtimeUAvg += runtimeU
        runtimeSAvg += runtimeS
        runtimeTAvg += runtimeT
            
        if perf:
            for line in err.split("\n"):
                if "samples" in line:
                    raw = line.split("samples")[0].split("(")[1].strip()
                    perfSampAvg += int(raw)
                    break

    runtimeUAvg /= nRuns
    runtimeSAvg /= nRuns
    runtimeTAvg /= nRuns
    if perf:
        perfSampAvg /= nRuns

    fOut.write(
        "AVG\n"
        + "{:.3f}".format(runtimeUAvg)
        + " "
        + "{:.3f}".format(runtimeSAvg)
        + " "
        + "{:.3f}".format(runtimeTAvg)
        + (" - {}".format(perfSampAvg) if perf else "")
        + "\n\n"
    )


def exec_sys_cmd(nRuns, fOut, cmdStress, tmpFile=False, perf=False):
    runtimeUAvg = 0
    runtimeSAvg = 0
    runtimeTAvg = 0
    perfSampAvg = 0

    for run in range(nRuns):
        wkExec = cmdStress
        print("EXEC:", " ".join(wkExec))

        if tmpFile:
            p = cmd.dcmd(" ".join(wkExec + ["2>", "tmpFile.txt"]), sh=True)
            p.wait()

            tmpF = open("tmpFile.txt", "r")
            err = "".join(tmpF.readlines())
            tmpF.close()
            cmd.cmd("rm tmpFile.txt", sh=True)
            out = ""
            ret = 0
        else:
            out, err, ret = cmd.cmd(wkExec)

        if ret:
            print("Error while exec: ", " ".join(wkExec))
            print("OUT", out)
            print("ERR", err)
            exit(-1)

        parse = False

        runtimeT = time
        for line in out.split("\n"):
            # if "execution time" in line:
            #     print(line)
            #     runtimeT = float(line.split("/")[1].split()[-1])
            if "avg:" in line:
                print(line)
                runtimeT = float(line.split()[1])
       
        print("OUT", out)
        runtimeU = 0
        runtimeS = 0
        # runtimeT = getTimeFromStressOutput(err, "total time")

        runtimeUAvg += runtimeU
        runtimeSAvg += runtimeS
        runtimeTAvg += runtimeT

        if perf:
            for line in err.split("\n"):
                if "samples" in line:
                    raw = line.split("samples")[0].split("(")[1].strip()
                    perfSampAvg += int(raw)
                    break

    runtimeUAvg /= nRuns
    runtimeSAvg /= nRuns
    runtimeTAvg /= nRuns
    if perf:
        perfSampAvg /= nRuns

    fOut.write(
        "AVG\n"
        + "{:.3f}".format(runtimeUAvg)
        + " "
        + "{:.3f}".format(runtimeSAvg)
        + " "
        + "{:.3f}".format(runtimeTAvg)
        + (" - {}".format(perfSampAvg) if perf else "")
        + "\n\n"
    )


def getTimeFromOutput(output, type):

    soutput = output.split(" ")

    for i in range(len(soutput)):
        # if soutput[i] == type:
        if type in soutput[i]:
            raw = soutput[i].replace(type, "")
            return float(raw)

    return 0


def exec_time_cmd(nRuns, fOut, cmdExec):
    runtimeUAvg = 0
    runtimeSAvg = 0
    runtimeTAvg = 0

    for run in range(nRuns):
        wkExec = ["time"] + cmdExec
        print(wkExec)
        out, err, ret = cmd.cmd(wkExec)

        runtimeU = getTimeFromOutput(err, "user")
        runtimeS = getTimeFromOutput(err, "system")
        runtimeT = runtimeU + runtimeS

        runtimeUAvg += runtimeU
        runtimeSAvg += runtimeS
        runtimeTAvg += runtimeT

    runtimeUAvg /= nRuns
    runtimeSAvg /= nRuns
    runtimeTAvg /= nRuns

    fOut.write(
        "AVG\n"
        + "{:.3f}".format(runtimeUAvg)
        + " "
        + "{:.3f}".format(runtimeSAvg)
        + " "
        + "{:.3f}".format(runtimeTAvg)
        + "\n\n"
    )


def plot_4_2_2(data, nrPmus, index, title):

    sns.set_style("darkgrid")
    sns.set_color_codes("pastel")

    fig, axes = plt.subplots(ncols=len(nrPmus))
    # fig.legend(loc=7)

    if len(index) == 1:
        axes = axes.ravel()

    for i in range(len(nrPmus)):

        df = pd.DataFrame(
            data[i],
            index=index,
        )

        for column in df:
            if (column == "DUMMY"):
                continue
            df[column] = ((df[column] / df["DUMMY"]) - 1) * 100
            print(df[column])
            print(df["DUMMY"])
            print(column)
           
        print(df)
        df.drop('DUMMY', axis=1, inplace=True)

        # plotdata.loc[plotdata["KPROBE"] < 0, "KPROBE"] = 0
        # plotdata.loc[plotdata["TRACEPOINT"] < 0, "TRACEPOINT"] = 0

        """ Remove to get absolute values """
        # minValuesObj = df.min(axis=1)
        # for column in df:
        #     df[column] = (df[column] / minValuesObj) - 1

        print("df", df)

        df.plot(
            ax=axes[i], kind="bar", figsize=(18.5, 8.5), rot=45, alpha=0.6, width=0.75
        )

        # axes[i].legend(loc='lower left', bbox_to_anchor=(0, 1.02), fancybox=True, shadow=True, ncol=3, fontsize=12)
        axes[i].set_title("Stressor instance: " + nrPmus[i])
        axes[i].get_legend().remove()

        # for p in axes[i].patches:
        #     if (p.get_height() > 0):
        #         axes[i].annotate("{:.2f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=10)
        #     else:
        #         axes[i].annotate("{:.0f}".format(p.get_height()), (0.1, .02), fontsize=10)
        axes[i].set_ylabel("Overhead (%)", rotation=90, fontsize=10, fontweight='bold')

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        bbox_to_anchor=(0.515, 1.005),
        loc="upper center",
        ncol=3,
        fontsize=12,
    )

    # plt.suptitle(title, fontsize=14)


    # plt.suptitle(title, fontsize=14)
    # plt.title('Executed with stress-ng "switch" stressor', fontsize=10, fontweight='bold')

    # plt.ylabel("Overhead (%)", fontsize=10, fontweight='bold')


    fig.set_size_inches(16, 5)
    plt.yticks(fontsize=10)
    plt.xticks(fontsize=10)

    # plt.suptitle(title, fontsize=12, fontweight='bold')
    # plt.show()

    plt.savefig('filter.pdf', bbox_inches='tight', pad_inches=0)


def parse_and_plot_4_2_2(args):

    # Parse
    fIn = openFile(DATA_DIR + "jbase_4_2_2.txt", "r")
    if fIn is None:
        return

    pmu = 0
    plug = ""
    tracker = ""
    workloads = []

    dataPmu = []
    ready = False

    dataAgg = {}

    for line in fIn.readlines():

        if "PLUG" in line:
            plug = line.split("PLUG")[1].strip()
        elif "TRACKER" in line:
            tracker = line.split("TRACKER")[1].strip()
        elif "FREQ" in line:
            dataPmu = []
            freq = line.split("FREQ")[1].strip()
            if (plug + "-" + freq) not in dataAgg:
                dataAgg[plug + "-" + freq] = []
        elif "PMU" in line:
            pmu = line.split("PMU")[1].strip()
            if pmu not in dataPmu:
                dataPmu.append(pmu)
                dataAgg[plug + "-" + freq].append({"DUMMY": [], "HASH": [], "PERF": [], "STACK": []})
        elif "WK" in line:
            wk = line.split("WK")[1].strip()
            if wk not in workloads:
                workloads.append(wk)
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            times = line.strip().split(" ")
            dataAgg[plug + "-" + freq][dataPmu.index(pmu)][tracker].append(
                float(times[2]) * 1000
            )
            ready = False

    """ ONE PLOT (Fixed PLUG and FREQ), variable workloads, trackers and pmcs """

    for x in dataAgg:
        dataAgg[x] = dataAgg[x][: len(dataPmu)]

    for x in dataAgg:
        print(x)
        for y in dataAgg[x]:
            print(y)

    print(dataPmu)

    for x in dataAgg:
        title = "Workload runtime comparison using different Query Strategies"
        plot_4_2_2(dataAgg[x], dataPmu, workloads, title)


def jbase_4_2_2(args):
    fOut = openFile("results/jbase_4_2_2.txt", "w")
    if fOut is None:
        return

    chdir("/home/userx/git/SHook")
    cmd.cmd("make clean", sh=True)

    chdir("/home/userx/git/SHook/client")

    frequencies = ["20", "16", "12"]
    plugins = ["KPROBE", "TRACEP"]
    trackers = ["HASH", "PERF", "STACK", "DUMMY"]
    nr_pmus = [1]
    nRuns = 3

    frequencies = ["15"]
    plugins = ["TRACEP"]
    nr_pmus = [1]
    nRuns = 1

    cmd.cmd("make clean", sh=True)

    _cmd_wrapper = ["../tools/wrapper"]
    _cmd_stress = ["stress-ng", "--times"]

    _cmd_stress = ["sudo", "stress-ng", "--times", "--metrics-brief"]

    for plug in plugins:

        fOut.write("PLUG " + plug + "\n")

        for tracker in trackers:

            fOut.write("TRACKER " + tracker + "\n")
            out, err, ret = cmd.cmd("make PLUGIN=" + plug + " TRACKER=" + tracker, sh=True)

            print(err)
            for freq in frequencies:
                fOut.write("FREQ " + str(freq) + "\n")

                for pmu in nr_pmus:
                    fOut.write("PMU " + str(pmu) + "\n")
                    cmd.cmd("make unload", sh=True)
                    out, err, ret = cmd.cmd(
                        'make load PARAMS="readable_pmcs='
                        + str(pmu)
                        + " reset_period="
                        + str(freq)
                        + '"',
                        sh=True,
                    )

                    print(err)

                    for wk in workloads:
                        print("*** REMOVE CPU FILTER ***")
                        if wk != "matrix-3d":
                            continue

                        wkExec = (
                            _cmd_wrapper
                            + _cmd_stress
                            + (
                                "--"
                                + wk
                                + " "
                                + str(1)
                                + " --"
                                + wk
                                + "-ops "
                                + str(int(int(workloads[wk]) / 10))
                            ).split(" ")
                        )

                        fOut.write("WK " + wk + "\n")
                        exec_cmd(nRuns, fOut, wkExec)

                    fOut.flush()

    cmd.cmd("make unload", sh=True)
    fOut.close()


def jbase_4_3_1(args):
    fOut = openFile("results/jbase_4_3_1.txt", "w")
    if fOut is None:
        return

    chdir("/home/userx/git/SHook/client")

    frequencies = ["20", "16", "12"]
    plugins = ["KPROBE", "TRACEP"]
    trackers = ["PERF", "STACK", "HASH"]
    bufferings = ["SYSTEM", "THREAD", "CPU"]
    # clist = ["ARRAY", "NBLIST"]
    clist = ["ARRAY", "DUMMY", "NBLIST"]

    nr_pmus = [8, 4, 1]
    nRuns = 3

    plugins = ["TRACEP"]
    trackers = ["HASH"]
    frequencies = ["14"]
    # bufferings = ["THREAD"]
    nr_pmus = [8]

    dummySkip = False

    _cmd_wrapper = ["../tools/wrapper"]
    _cmd_stress = ["sudo", "time", "stress-ng", "--times", "--metrics-brief"]
    # _cmd_stress = "sysbench --test=cpu --cpu-max-prime=100000 --threads=1 --time=5 run".split()

    for plug in plugins:

        fOut.write("PLUG " + plug + "\n")

        for tracker in trackers:

            fOut.write("TRACKER " + tracker + "\n")

            for buffer in bufferings:
                fOut.write("BUFFERING " + buffer + "\n")

                for cl in clist:
                    fOut.write("CLIST " + cl + "\n")

                    if cl == "NBLIST" and buffer != "CPU":
                        continue

                    out, err, ret = cmd.cmd(
                        "make PLUGIN="
                        + plug
                        + " TRACKER="
                        + tracker
                        + " BUFFERP="
                        + buffer
                        + " CLIST="
                        + cl,
                        sh=True,
                    )

                    if ret != 0:
                        print("ERROR while compiling 4_3_1")
                        print("ERR", err)
                        print("OUT", out)
                        exit(1)

                    for freq in frequencies:
                        fOut.write("FREQ " + str(freq) + "\n")

                        for pmu in nr_pmus:
                            fOut.write("PMU " + str(pmu) + "\n")
                            cmd.cmd("make unload", sh=True)
                            cmd.cmd(
                                'make load PARAMS="readable_pmcs='
                                + str(pmu)
                                + " reset_period="
                                + str(freq)
                                + '"',
                                sh=True,
                            )

                            print("**", buffer, cl, freq)

                            for wk in workloads:
                                # print("*** REMOVE CPU FILTER ***")
                                # if wk != "cache":
                                #     continue

                                wkExec = (
                                    _cmd_wrapper
                                    + _cmd_stress
                                    + (
                                        "--"
                                        + wk
                                        + " "
                                        + str(SMT)
                                        + " --"
                                        + wk
                                        + "-ops "
                                        + str(int(int(workloadsSmt[wk])))
                                    ).split(" ")
                                )

                                fOut.write("WK " + wk + "\n")

                                if "stress-ng" in wkExec:
                                    exec_cmd(nRuns, fOut, wkExec)
                                else:
                                    exec_sys_cmd(nRuns, fOut, wkExec)

                            fOut.flush()

    cmd.cmd("make unload", sh=True)
    fOut.close()


def jbase_4_3_2(args):
    fOut = openFile("results/jbase_4_3_2.txt", "w")
    if fOut is None:
        return

    chdir("perf-5.4.127")

    _cmd_perf = ["sudo", "./perf", "record"]
    _cmd_stress = ["stress-ng", "--times"]

    perfBase = ["--stat", "--raw-samples", "--overwrite"]
    perfBase = ["--stat", "--raw-samples"]

    smtProc = open("/sys/devices/system/cpu/smt/active", "r")
    smtOn = int(smtProc.readline())
    smtProc.close()

    perfOpts = [True]

    perfEvts = "cycles/period={}/,r010e/period=0/,r019c/period=0/,r02c2/period=0/"

    if not smtOn:
        perfEvts += ",r40a6/period=0/,r01a6/period=0/,r02a6/period=0/,r04a6/period=0/"

    perfSamp = [2 ** 20, 2 ** 16, 2 ** 12]
    # perfBuff = [2 ** 4, 2 ** 8, 2 ** 16]
    perfBuff = [2 ** 4]

    nRuns = 3

    fOut.write("SMT_ON " + str(smtOn) + "\n")

    for samp in perfSamp:
        fOut.write("PERIOD " + str(samp) + "\n")

        # for buff in perfBuff:
        #     fOut.write("BSIZE " + str(samp) + "\n")

        for wk in workloads:
            # print("*** REMOVE CPU FILTER ***")
            # if wk != "cpu" and wk != "io":
            #     continue

            for opts in perfOpts:
                fOut.write("BUFFERING " + str(opts) + "\n")

                wkExec = (
                    _cmd_perf
                    + ('-e "{' + perfEvts.format(samp) + '}"').split(" ")
                    + (perfBase if opts else (perfBase + ["--no-buffering"]))
                    # + ["-m", str(buff)]
                    + ["--"]
                    + _cmd_stress
                    + (
                        "--" + wk + " 1 --" + wk + "-ops " + str(workloads[wk])
                    ).split(" ")
                )

                print(" ".join(wkExec))

                fOut.write("WK " + wk + "\n")
                exec_cmd(nRuns, fOut, wkExec, True, True)

        fOut.flush()


def jbase_4_4_1(args):
    fOut = openFile("results/jbase_4_4_1.txt", "w")
    if fOut is None:
        return

    chdir("/home/userx/git/SHook/breader")

    stride = [2 ** 3, 2 ** 5, 2 ** 7, 2 ** 9, 2 ** 11, 2 ** 12]
    times = [1, 2, 4, 8]

    types = {"SEQ": "0", "RAW": "1", "MMAP": "2"}

    nRuns = 3

    # stride = [2 ** 3, 2 ** 5]
    # times = [1, 2]
    # types = {"SEQ": "0", "RAW": "1", "MMAP": "2"}
    # nRuns = 1

    _cmd_reader = ["../tools/reader"]

    # cmd.cmd("make unload", sh=True)
    cmd.cmd("make", sh=True)
    cmd.cmd("make load", sh=True)

    for type in types:
        fOut.write("TYPE " + type + "\n")

        for st in stride:

            fOut.write("STRIDE " + str(st) + "\n")

            for tm in times:

                fOut.write("TIMES " + str(tm) + "\n")

                wkExec = _cmd_reader + (
                    types[type] + " " + str(tm) + " " + str(st)
                ).split(" ")

                exec_time_cmd(nRuns, fOut, wkExec)

                fOut.flush()

    # cmd.cmd("make unload", sh=True)
    fOut.close()



from matplotlib.ticker import StrMethodFormatter

# plt.gca().yaxis.set_major_formatter(StrMethodFormatter('{x:,.0f}')) # No decimal places

def plot_4_1_1(plotdata, title, ylabel, freq, name, decimal=False):
    sns.set_style("darkgrid")
    sns.set_color_codes("pastel")

    ax = plotdata.plot(kind="bar", figsize=(18.5, 10.5), rot=45, alpha=0.6, width=0.7)
    ax.get_figure().set_size_inches(8, 4.5)

    max = 0
    for v in plotdata.max():
        if max < v and v != inf:
            max = v

    print(max)
    ax.set_ylim([0, max * 1.05])

    ax.legend(
        loc="lower right",
        bbox_to_anchor=(1, 1),
        fancybox=True,
        ncol=2,
        fontsize=14,
    )

    # for p in ax.patches:
    #     if (p.get_height() > 0):
    #         if decimal:
    #             ax.annotate("{:.2f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=8)
    #         else:
    #             ax.annotate("{:.0f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=8)
    #     else:
    #         ax.annotate("{:.0f}".format(p.get_height()), (0.1, .02), fontsize=8)

    if decimal:
        plt.gca().yaxis.set_major_formatter(StrMethodFormatter('{x:,.1f}'))
    else:
        plt.gca().yaxis.set_major_formatter(StrMethodFormatter('{x:,.0f}'))


    # plt.suptitle(title, fontsize=12, fontweight='bold')
    # plt.title("PMI Period: 2^" + freq + " clock cycles", fontsize=10, fontweight='bold')


    plt.ylabel(ylabel, fontsize=12, fontweight='bold')
    plt.yticks(fontsize=12)
    plt.xticks(fontsize=12)
    # plt.show()
    plt.savefig(name + "_" + freq + '.pdf', bbox_inches='tight', pad_inches=0)


def parse_and_plot_4_1_1(args):
    fileList = glob.glob(DATA_DIR + "jbase_4_1_*.txt")

    fileList.sort(reverse=True)

    for file in fileList:
        __parse_and_plot_4_1_1(file)


def __parse_and_plot_4_1_1(file):

    # Parse
    fIn = openFile(file, "r")
    if fIn is None:
        return

    frequency = file.replace(".txt", "").split("_")[-1]

    workloads = []
    dataTime = {"NMI": [], "IRQ": []}
    dataSample = {"NMI": [], "IRQ": []}
    dataEff = {"NMI": [], "IRQ": []}

    wk = ""
    vector = ""
    ready = False

    for line in fIn.readlines():

        if "VECTOR" in line:
            vector = line.split("VECTOR")[1].strip()
        elif "WK" in line:
            wk = line.split("WK")[1].split("@")[0].strip()
            if wk not in workloads:
                workloads.append(wk)
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            times, values = line.split("-")
            times = times.strip().split(" ")
            values = values.strip().split(" ")
            dataTime[vector].append(float(times[2]) * 1000)
            dataSample[vector].append(float(values[0]))
            dataEff[vector].append(float(values[1]) / float(times[2]))
            ready = False

    print(dataTime)

    dfTime = pd.DataFrame(
        dataTime,
        index=workloads,
    )

    baseline = []
    for wk in workloads:
        baseline.append(float(WORKLOADS[wk][2]) * 1000)

    dfBase = pd.DataFrame(
        {"base": baseline},
        index=workloads,
    )

    print(dfBase)

    dfTime["IRQ"] = ((dfTime["IRQ"] / dfBase["base"]) - 1) * 100
    dfTime["NMI"] = ((dfTime["NMI"] / dfBase["base"]) - 1) * 100

    dfTime.loc[dfTime["IRQ"] < 0, "IRQ"] = 0
    dfTime.loc[dfTime["NMI"] < 0, "NMI"] = 0

    # dfTime.drop("OFF", axis=1, inplace=True)

    title = "Workload overhead comparison between IRQ- and NMI-based PMIs"
    plot_4_1_1(dfTime, title, "Overhead (%)", frequency, "time", True)

    dfSample = pd.DataFrame(
        dataSample,
        index=workloads,
    )

    print("dfSample", dfSample)

    # dfSample.drop("OFF", axis=1, inplace=True)

    title = "Sample generation comparison between IRQ- and NMI-based PMIs"
    plot_4_1_1(dfSample, title, "Generated samples", frequency, "samples")

    dfEff = pd.DataFrame(
        {},
        index=workloads,
    )

    dfEff["NMI"] = dfSample["NMI"] / dfTime["NMI"]
    dfEff["IRQ"] = dfSample["IRQ"] / dfTime["IRQ"]

    title = "Efficiency between IRQ- and NMI-based PMIs"
    plot_4_1_1(dfEff, title, "Score (Samples/Overhead)", frequency, "efficiency")


def plot_4_2_1(data, labels, wk):
    title = "Context-Switch stress-test with different Hook strategies - " + wk
    sns.set_style("darkgrid")
    sns.set_color_codes("pastel")

    plotdata = pd.DataFrame(
        data,
        index=labels,
    )

    plotdata["KPROBE"] = ((plotdata["KPROBE"] / plotdata["NONE"]) - 1) * 100
    plotdata["TRACEPOINT"] = ((plotdata["TRACEPOINT"] / plotdata["NONE"]) - 1) * 100

    plotdata.loc[plotdata["KPROBE"] < 0, "KPROBE"] = 0
    plotdata.loc[plotdata["TRACEPOINT"] < 0, "TRACEPOINT"] = 0

    plotdata.drop("NONE", axis=1, inplace=True)

    ax = plotdata.plot(kind="bar",  rot=45, alpha=0.6, width=0.5)

    ax.legend(
        loc="lower right",
        bbox_to_anchor=(1, 1),
        fancybox=True,
        ncol=2,
        fontsize=14,
    )

    print("E")
    # for p in ax.patches:
    #     if (p.get_height() > 0):
    #         ax.annotate("{:.2f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=10)
    #     else:
    #         ax.annotate("{:.0f}".format(p.get_height()), (0.1, .02), fontsize=10)

    # plt.suptitle(title, fontsize=14)
    # plt.title('Executed with stress-ng "switch" stressor', fontsize=10, fontweight='bold')

    plt.ylabel("Overhead (%)", fontsize=13, fontweight='bold')

    ax.get_figure().set_size_inches(8, 5)
    plt.yticks(fontsize=12)
    plt.xticks(fontsize=12)
    # plt.show()
    plt.savefig('ctx.pdf', bbox_inches='tight', pad_inches=0)

def parse_and_plot_4_2_1(args):

    # Parse

    # labels = ["User", "System", "Total"]
    labels = ["switch", "fork", "clone"]

    plug = ""
    ready = False

    dataAgg = {"DUMMY": [], "KPROBE": [], "TRACEP": []}

    for wk in labels:
        data = {"DUMMY": [], "KPROBE": [], "TRACEP": []}
        
        fIn = openFile(DATA_DIR + "jbase_4_2_1.txt", "r")
        if fIn is None:
            return
        
        for line in fIn.readlines():
            if "stress" in line:
                enable = wk in line

            if not enable:
                continue

            if "PLUG" in line:
                plug = line.split("PLUG")[1].strip()
            elif "AVG" in line:
                ready = True
                continue
            elif ready:
                times = line.strip().split(" ")
                data[plug].append(float(times[0]) * 1000)
                data[plug].append(float(times[1]) * 1000)
                data[plug].append(float(times[2]) * 1000)
                ready = False

        fIn.close()
        # print(data)

        for k in data:
            dataAgg[k].append(data[k][2])

        # plot_4_2_1(data, labels, wk)
    dataAgg["NONE"] = dataAgg.pop("DUMMY")
    dataAgg["TRACEPOINT"] = dataAgg.pop("TRACEP")
    plot_4_2_1(dataAgg, labels, wk)


def plot_4_3_1(data, baseline, nrPmus, index, title):
    sns.set_style("darkgrid")
    sns.set_color_codes("pastel")

    fig, axes = plt.subplots(ncols=len(nrPmus))

    if len(nrPmus) == 1:
        axes = [axes]

    print("Plotting: ", data)

    for i in range(len(nrPmus)):

        bs = pd.DataFrame(
            baseline[i],
            index=index,
        )

        df = pd.DataFrame(
            data[i],
            index=index,
        )

        """ Remove to get absolute values """
        # minValuesObj = df.min(axis=1)
        for column in df:
            print(column)
            # df[column] = (df[column] / minValuesObj) - 1
            if column == "NBLIST":
                df[column] = ((df[column] / bs["CPU"]) - 1) * 100
            else:
                df[column] = ((df[column] / bs[column]) - 1) * 100

        df.plot(
            ax=axes[i], kind="bar", rot=45, alpha=0.6, width=0.75
        )

        # axes[i].legend(loc='lower left', bbox_to_anchor=(0, 1.02), fancybox=True, shadow=True, ncol=3, fontsize=12)
        axes[i].set_title("Stressor instance: " + nrPmus[i])
        axes[i].get_legend().remove()

        # for p in axes[i].patches:
        #     if (p.get_height() > 0):
        #         axes[i].annotate("{:.2f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=10)
        #     else:
        #         axes[i].annotate("{:.0f}".format(p.get_height()), (0.1, .02), fontsize=10)
        axes[i].set_ylabel("Overhead (%)", rotation=90, fontsize=12, fontweight='bold')

    handles, labels = axes[0].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        bbox_to_anchor=(0.515, 1.02),
        loc="upper center",
        ncol=4,
        fontsize=14,
    )

    fig.set_size_inches(16, 5)
    # plt.yticks(fontsize=12)
    # plt.xticks(fontsize=12)

    # plt.suptitle(title, fontsize=12, fontweight='bold')
    # plt.show()

    plt.savefig('buffering.pdf', bbox_inches='tight', pad_inches=0)


def parse_and_plot_4_3_1(args):

    # Parse
    fIn = openFile(DATA_DIR + "jbase_4_3_1.txt", "r")
    if fIn is None:
        return

    pmu = 0
    cl = ""
    frequency = 0
    plug = ""
    tracker = ""
    buffering = ""
    workloads = []

    dataPmu = []
    dataAgg = {}

    ready = False

    for line in fIn.readlines():
        # if "PLUG" in line:
        #     plug = line.split("PLUG")[1].strip()
        # elif "TRACKER" in line:
        #     tracker = line.split("TRACKER")[1].strip()
        # elif "FREQ" in line:
        if "FREQ" in line:
            dataPmu = []
            freq = line.split("FREQ")[1].strip()
            if (cl + "-" + freq) not in dataAgg:
                dataAgg[cl + "-" + freq] = []
        elif "PMU" in line:
            pmu = line.split("PMU")[1].strip()
            if pmu not in dataPmu:
                dataPmu.append(pmu)
                dataAgg[cl + "-" + freq].append({"SYSTEM": [], "CPU": [], "THREAD": []})
        elif "WK" in line:
            wk = line.split("WK")[1].strip()
            if wk not in workloads:
                workloads.append(wk)
        elif "BUFFERING" in line:
            buffering = line.split("BUFFERING")[1].strip()
        elif "CLIST" in line:
            cl = line.split("CLIST")[1].strip()
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            times = line.strip().split(" ")
            dataAgg[cl + "-" + freq][dataPmu.index(pmu)][buffering].append(
                float(times[2]) * 1000
            )
            ready = False

    """ ONE PLOT (Fixed PLUG, FREQ, TRACKER), variable workloads, buffering and pmcs """

    for x in dataAgg:
        dataAgg[x] = dataAgg[x][: len(dataPmu)]

    dataTmp = {}
    for x in dataAgg:
        if "NBLIST" in x:
            fq = x.split("-")[1]
            for i in range(len(dataAgg[x])):
                dataAgg["ARRAY-" + fq][i]["NBLIST"] = dataAgg[x][i]["CPU"]
        else:
            dataTmp[x] = dataAgg[x]

    dataAgg = dataTmp

    dataBaseline = {}
    for x in dataAgg:
        if "DUMMY" in x:
            dataBaseline[x] = dataAgg[x]

    for x in dataBaseline:
        dataAgg.pop(x)

    # print ("dataAgg", dataAgg)
    # print ("dataBaseline", dataBaseline)

    for x in dataAgg:
        print(x)
        for y in dataAgg[x]:
            print(y)

    for x in dataAgg:
        title = "Workload runtime comparison using different Buffering Policies"
        title += "\nPMI Period: 2^" + x.split("-")[1] + " clock cycles"
        print("Plotting:", x)
        plot_4_3_1(dataAgg[x], dataBaseline["DUMMY-" + x.split("-")[1]], dataPmu, workloads, title)


def plot_4_3_2(data, baseline, index, title, ylabel, rev=None):
    sns.set_style("darkgrid")
    sns.set_color_codes("pastel")

    print("rev", rev)

    pBaseline = pd.DataFrame(
        {"baseline": baseline},
        index=index,
    )

    plotdata = pd.DataFrame(
        data,
        index=index,
    )

    if rev is not None:
        bs = pd.DataFrame(
            rev,
            index=index,
        )

    if baseline is not None:
        for column in plotdata:
            if rev is None:
                plotdata[column] = ((plotdata[column] / pBaseline["baseline"]) - 1) * 100
            else:
                bs[column] = ((bs[column] / pBaseline["baseline"]) - 1) * 100
                plotdata[column] = (plotdata[column] / bs[column])

    # plotdata[""]
    print(plotdata)
    print(pBaseline)

    ax = plotdata.plot(kind="bar", figsize=(18.5, 10.5), rot=45, alpha=0.6, width=0.75)

    ax.legend(
        loc="lower right",
        bbox_to_anchor=(1, 1),
        fancybox=True,
        ncol=3,
        fontsize=14,
    )

    # for p in ax.patches:
    #     if (p.get_height() > 0):
    #         if baseline is not None and rev is None:
    #             ax.annotate("{:.2f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=8)
    #         else:
    #             ax.annotate("{:.0f}".format(p.get_height()), (p.get_x() + 0.02, p.get_height() * 1.005), fontsize=8)
    #     else:
    #         ax.annotate("{:.0f}".format(p.get_height()), (0.1, .02), fontsize=8)

    # plt.suptitle(title, fontsize=14)
    # plt.title('Executed with stress-ng "switch" stressor', fontsize=12)


    plt.ylabel(ylabel, fontsize=12, fontweight='bold')
    plt.yticks(fontsize=12)
    plt.xticks(fontsize=12)

    ax.get_figure().set_size_inches(8, 4)

    # plt.show()

    # plt.show()
    plt.savefig(title + '.pdf', bbox_inches='tight', pad_inches=0)


WORKLOADS = {}
WORKLOADS_SMT = {}


def parse_baseline():
    fIn = openFile(DATA_DIR + "jbase_4_0_0.txt", "r")
    if fIn is None:
        return

    ready = False
    baseline = WORKLOADS

    for line in fIn.readlines():
        if "MOD" in line:
            mod = int(line.split("MOD")[1].strip())
            if mod == 1:
                baseline = WORKLOADS
            else:
                baseline = WORKLOADS_SMT
        elif "WK" in line:
            wk = line.split("WK")[1].split("@")[0].strip()
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            times = line.strip().split(" ")
            baseline[wk] = times
            ready = False

    fIn.close()


def parse_and_plot_4_3_2(args):

    # Parse
    fIn = openFile(DATA_DIR + "jbase_4_3_2.txt", "r")
    if fIn is None:
        return

    period = ""
    bsize = ""
    buffering = ""
    workloads = []
    dataAgg = {"True": {}, "False": {}}
    dataSamples = {"True": {}, "False": {}}
    dataEff = {"True": {}, "False": {}}

    ready = False

    for line in fIn.readlines():
        if "PERIOD" in line:
            period = line.split("PERIOD")[1].strip()
            period = "2^" + str(int(math.log(int(period), 2)))

            dataAgg["True"][period] = []
            dataAgg["False"][period] = []
            dataSamples["True"][period] = []
            dataSamples["False"][period] = []
            dataEff["True"][period] = []
            dataEff["False"][period] = []
        # elif "BSIZE" in line:
        #     bsize = line.split("BSIZE")[1].strip()
        elif "BUFFERING" in line:
            buffering = line.split("BUFFERING")[1].strip()
            if period not in dataAgg[buffering]:
                dataAgg[buffering][period] = []
            if period not in dataSamples[buffering]:
                dataSamples[buffering][period] = []
            if period not in dataEff[buffering]:
                dataEff[buffering][period] = []
        elif "WK" in line:
            wk = line.split("WK")[1].strip()
            if wk not in workloads:
                workloads.append(wk)
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            times = line.strip().split(" ")
            values = line.strip().split("-")[1]
            dataAgg[buffering][period].append(float(times[2]) * 1000)
            dataSamples[buffering][period].append(float(values))
            dataEff["True"][period].append(float(values))
            dataEff["False"][period].append((float(times[2]) * 1000))
            ready = False

    baseline = []
    for wk in workloads:
        baseline.append(float(WORKLOADS[wk][2]) * 1000)

    dataBufOn = dataAgg["True"]
    dataBufOff = dataAgg["False"]

    # for sk in dataBufOn:
    # print(sk, dataBufOn[sk])
    title = "Workload runtime using Perf record on clock cycles"
    title += "\nPeriod: 2^" + str(int(math.log(int(2), 2))) + " - buffering ON"
    plot_4_3_2(dataBufOn, baseline, workloads, "perf_time", "Overhead (%)")

    # for sk in dataBufOff:
    #     print(dataBufOff[sk])
    #     title = "Workload runtime using Perf record on clock cycles"
    #     title += "\nPeriod: 2^" + str(int(math.log(int(sk), 2))) + " - buffering OFF"
    #     plot_4_3_2(dataBufOff[sk], baseline, workloads, title, "Overhead (%)")

    dataBufOn = dataSamples["True"]
    dataBufOff = dataSamples["False"]

    # for sk in dataBufOn:
    #     print(dataBufOn[sk])
    title = "Workload runtime using Perf record on clock cycles"
    title += "\nPeriod: 2^" + str(int(math.log(int(2), 2))) + " - buffering ON"
    plot_4_3_2(dataBufOn, None, workloads, "perf_samples", "Generated samples")


    title = "Workload runtime using Perf record on clock cycles"
    title += "\nPeriod: 2^" + str(int(math.log(int(2), 2))) + " - EFF"
    plot_4_3_2(dataEff["True"], baseline, workloads, "perf_efficiency", "Score (Samples/Overhead)", dataEff["False"])

    # for sk in dataBufOff:
    #     print(dataBufOff[sk])
    #     title = "Workload runtime using Perf record on clock cycles"
    #     title += "\nPeriod: 2^" + str(int(math.log(int(sk), 2))) + " - buffering OFF"
    #     plot_4_3_2(dataBufOff[sk], None, workloads, title, "Samples")

    """ ONE PLOT (Fixed SAMPL, BUFFERING), variable workloads, buff_size """


def parse_and_plot_4_4_1(args):

    # Parse
    fIn = openFile(DATA_DIR + "jbase_4_4_1.txt", "r")
    if fIn is None:
        return

    type = ""
    strides = []
    vtimes = ""

    dataVTimes = []
    dataVal = []
    ready = False

    for line in fIn.readlines():

        if "TYPE" in line:
            type = line.split("TYPE")[1].strip()
        elif "TIMES" in line:
            vtimes = line.split("TIMES")[1].strip()
            if vtimes not in dataVTimes and vtimes == "1":
                dataVTimes.append(vtimes)
                dataVal.append({"SEQ": [], "RAW": [], "MMAP": []})
        elif "STRIDE" in line:
            srd = line.split("STRIDE")[1].strip()
            if srd not in strides:
                strides.append(srd)
        elif "AVG" in line:
            ready = True
            continue
        elif ready:
            if vtimes == "1":
                times = line.strip().split(" ")
                dataVal[dataVTimes.index(vtimes)][type].append((float(times[2]) * 1000) / int(vtimes))
            ready = False

    """ ONE PLOT (Fixed TIMES), variable workloads, type and stride """

    print(dataVal)
    print(strides)
    print(dataVTimes)

    sns.set_color_codes("pastel")
    sns.set_style("darkgrid")

    fig, axes = plt.subplots(ncols=len(dataVTimes))

    if len(dataVTimes) == 1:
        axes = [axes]

    # colors = ["blue", "orange", "green", "purple", "black"]

    for i in range(len(dataVTimes)):
        df = pd.DataFrame(
            dataVal[i],
            index=strides,
        )
        df.plot(
            ax=axes[i],
            kind="bar",
            figsize=(18.5, 10.5),
            rot=45,
            alpha=0.6,
            width=0.75,
            # edgecolor="black",
            # color=colors,
            # linewidth=5,
        )
        # axes[i].set_title("Complete Buffer Reads: " + dataVTimes[i])
        axes[i].get_legend().remove()

    handles, labels = axes[-1].get_legend_handles_labels()
    fig.legend(
        handles,
        labels,
        bbox_to_anchor=(0.9, 0.98),
        loc="upper right",
        ncol=3,
        fontsize=14,
    )

    # title = "Performace comparison using different Kernel Access Points"
    # plt.suptitle(title, fontsize=14)

    # plt.title("Context-Switch Hook: " + plug + " - PMI Frequency: 2^" + freq + " clock cycles", fontsize=12)

    # for ax, row in zip(axes, ["Time (mSec)"]):
    #     ax.set_ylabel(row, rotation=90, fontsize=12)

    # for ax, col in zip(axes, dataVTimes):
    #     ax.set_xlabel("Request Size (Bytes) ", fontsize=12)

    plt.ylabel("Time (mSec)", fontsize=12, fontweight='bold')
    plt.xlabel("Request Size (Bytes)", fontsize=12, fontweight='bold')
        
    plt.yticks(fontsize=12)
    plt.xticks(fontsize=12)

    fig.set_size_inches(10, 6)

    # plt.show()
    plt.savefig('read.pdf', bbox_inches='tight', pad_inches=0)


def validate_args(args):
    return args.command == PLUGIN_NAME


DATA_DIR = "results/"


def compute(args):
    global DATA_DIR

    if not validate_args(args):
        return False

    chdir(app.globalConf.readPath("tools"))

    if args.directory:
        DATA_DIR = args.directory + "/"

    if args.parse:
        parse_baseline()
        if "all" in args.parse:
            args.parse = [
                "4_1_1",
                "4_2_1",
                "4_2_2",
                "4_3_1",
                "4_3_2",
                "4_4_1",
            ]
        for p in args.parse:
            globals()["parse_and_plot_" + p](p)

    if args.calibrate:
        calibrate()

    if args.exec_bench:
        print("**** ENABLE CALIBRATION ****")
        if "all" in args.exec_bench:
            args.exec_bench = [
                "4_0_0",
                "4_1_1",
                "4_2_1",
                "4_2_2",
                "4_3_1",
                "4_3_2",
                "4_4_1",
            ]
        for eb in args.exec_bench:
            chdir(app.globalConf.readPath("tools"))
            globals()["jbase_" + eb](args)

    return True
