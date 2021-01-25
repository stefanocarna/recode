#!/bin/python3
import argparse
from enum import IntEnum
import os
from pathlib import Path
import subprocess
import sys

# sys.path.insert(0, './utility')
import utility.plot as plot


class Pipe(IntEnum):
    OUT = 0
    ERR = 1
    RET = 2


def cmd(cmd_seq, type=None, sh=False, timeOut=None):
    try:
        p = subprocess.Popen(
            cmd_seq, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=sh
        )
        p.wait(timeOut)
        if type == Pipe.OUT or type == Pipe.ERR:
            return p.communicate()[type].decode("ASCII")
        elif type == Pipe.RET:
            return p.returncode
        else:
            out, err = p.communicate()
            return out.decode("ASCII"), err.decode("ASCII"), p.returncode
    except subprocess.TimeoutExpired:
        print("Process automatically teminated after " + str(timeOut) + " sec")
        p.terminate()
        return "** Timeout: " + str(timeOut) + " sec **", "", 0
    except Exception as e:
        print("An error occurred:" + str(e))
        return "", "", -1


def parser_init():
    parser = argparse.ArgumentParser(description="Tool to interact with Recode Module")

    parser.add_argument(
        "-m",
        "--module",
        type=str,
        required=False,
        choices=["c", "i", "r", "ci", "rci"],
        help="Specify a module action: (c)ompile, (i)nstall, (r)emove",
    )

    parser.add_argument(
        "-s",
        "--state",
        type=str,
        required=False,
        choices=["off", "tuning", "profile", "system", "idle", "reset"],
        help="Set the Recode state: (on) enabled, (off) disabled, (reset) off -> on",
    )

    parser.add_argument(
        "-f",
        "--frequency",
        metavar="F",
        type=int,
        required=False,
        help="Set the Recode profiling (F)requency",
    )

    parser.add_argument(
        "-i",
        "--info",
        action="store_true",
        required=False,
        help="Get current Recode configuration",
    )

    # parser.add_argument('-x', '--exec', metavar='prog', nargs='+', required=False,
    # 	help="Execute and profile a given program. Specific program arguments inline")

    parser.add_argument(
        "-x",
        "--exec",
        metavar="prog",
        type=str,
        required=False,
        help="Execute and profile a given program. Specific program arguments inline",
    )

    parser.add_argument(
        "-a",
        "--args",
        type=str,
        required=False,
        help="Execute and profile a given program. Specific program arguments inline",
    )

    parser.add_argument(
        "-t",
        "--timeout",
        metavar="T",
        type=int,
        required=False,
        help="Set an execution timeout",
    )

    parser.add_argument(
        "-c",
        "--cpu",
        type=int,
        required=False,
        help="Bind the program execution to a specific cpu",
    )

    parser.add_argument(
        "-p",
        "--plot",
        action="store_true",
        required=False,
        help="Plot profiling data",
    )

    subparser = parser.add_subparsers(help="command")
    test_parser = subparser.add_parser("test", help="Run tool test")

    test_parser.add_argument(
        "-m",
        "--monitor",
        action="store_true",
        required=False,
        help="Monitor the program",
    )

    test_parser.add_argument(
        "-p",
        "--plot",
        action="store_true",
        required="--monitor" in sys.argv,
        help="Plot the monitored program samples [Requires -m]",
    )

    test_parser.add_argument(
        "-t",
        "--tuning",
        action="store_true",
        required="--monitor" in sys.argv,
        help="Set Reocde in TUNING mode [Requires -m]",
    )

    test_parser.add_argument(
        "-a",
        "--attack",
        type=str,
        metavar="ATT",
        choices=["fr1", "fr2", "fr3", "fr4", "fr5", "pp1", "pp2", "ppl1", "ppl3", "xa", "xp"],
        required=False,
        help="Execute the attack",
    )

    conf_parser = subparser.add_parser("config", help="Configure the module")

    conf_parser.add_argument(
        "-m",
        "--mitigations",
        type=str,
        nargs='+',
        metavar="CONF",
        choices=["te", "exile", "llc", "verbose", "none"],
        required=False,
        help="Configure the module",
    )

    return parser


RECODE_PROC_PATH = "/proc/recode"


def check_recode():
    return os.path.isdir(RECODE_PROC_PATH)


def __action_module(action):
    if action == "c":
        action = "make"
        print("Compiling module...")
    elif action == "i":
        action = "make insert"
        print("Mounting module...")
    elif action == "r":
        action = "make remove"
        print("Unmounting module...")
    else:
        print("Something wrong: --module " + action + " illegal")

    out, err, ret = cmd(action, sh=True)

    if ret != 0:
        print("Module action failed with retCode " + str(ret))
        print("Error log: ")
        print(err)
    else:
        print("Done")

    return ret


def action_module(action):

    # Folder up
    os.chdir(Path(os.getcwd()).parent)

    print(os.getcwd())

    for c in action:
        if __action_module(c):
            break


def action_mitigations(action):
    
    path = RECODE_PROC_PATH + "/mitigations"

    _file = open(path, "w")

    mask = 0

    #define DM_G_LLC_FLUSH			0	
    #define DM_G_CPU_EXILE			1	
    #define DM_G_TE_MITIGATE		2
    #define DM_G_VERBOSE			18	

    if "llc" in action:
        mask |= (1 << 0)
    if "exile" in action:
        mask |= (1 << 1)
    if "te" in action:
        mask |= (1 << 2)
    if "verbose" in action:
        mask |= (1 << 18)
    if "none" in action:
        mask = 0

    _file.write(str(mask))
    _file.flush()
    _file.close()


def action_state(action):
    path = RECODE_PROC_PATH + "/state"

    _file = open(path, "w")

    if action == "off":
        value = "0"
    elif action == "tuning":
        value = "1"
    elif action == "profile":
        value = "2"
    elif action == "system":
        value = "3"
    elif action == "idle":
        value = "4"
    elif action == "reset":
        action_state("off")
        value = "2"
    else:
        print("Something wrong: --state " + action + " illegal")

    _file.write(value)
    _file.flush()
    _file.close()


def action_frequency(value):
    value = int(value)
    if value < 1 and value > 12:
        print("frequency is expressed as (1 << P) + 1")
        return

    pmc_mask = (1 << 48) - 1
    pmc_mask -= (1 << (value * 4)) - 1

    path = RECODE_PROC_PATH + "/frequency"

    _file = open(path, "w")

    _file.write(hex(pmc_mask))
    _file.flush()
    _file.close()


def get_info(info):
    if info not in ["frequency", "events", "thresholds"]:
        print("Cannot read " + info + " info. Returning 0")
        return 0

    path = RECODE_PROC_PATH + "/" + info
    _file = open(path, "r")
    value = _file.readlines()
    _file.close()
    return " ".join([str(e) for e in value]).strip()


def action_info():
    print("### Recode info")
    print(" - frequency:\t" + hex(int("0x" + get_info("frequency"), 0)))
    print(" - events:\t" + get_info("events"))
    print(" - thresholds:\t" + get_info("thresholds"))


def action_exec(prog, args, timeout, cpu, profile=True):
    wrapper = "wrapper"
    if not os.path.isfile(wrapper):
        # Compile Wrapper
        cmd("make")

    if timeout is not None and timeout < 0:
        print("Invalid timeout (< 0). Ignore timeout")
        timeout = None

    _cmd = ["./" + wrapper] if profile else []

    if cpu is not None:
        if cpu < 0 or cpu >= os.cpu_count():
            print("Invalid cpu " + str(cpu) + ". Ignore cpu mask")

        else:
            _cmd = ["taskset", "-c", str(cpu)] + _cmd

    _cmd = _cmd + [prog]

    if args is not None:
        _cmd = _cmd + args.split()

    print("Exec: " + str(_cmd) + " @ " + str(timeout))
    out, err, ret = cmd(_cmd, timeOut=timeout)

    if ret != 0:
        print("Execution failed with errcode " + str(ret))
        print("ERR pipe: " + str(err))
    else:
        print("OUT pipe: " + out)


def tool_plot():
    plot.plotCpusData()


def tool_test(test, tuning=False, monitor=False, plot=False):
    if test is None:
        print("Nothing to test. Specify an attack.")
        return

    if os.geteuid() != 0:
        answer = input("Attacks should be run as root. Proceed anyway? [y/N] ")
        if answer != "y":
            exit(0)

    if monitor:
        action_state("off")
        action_state("tuning" if tuning else "profile")
        action_frequency(5)

    # flush_flush
    if test == "fr1":
        prog = "attacks/flush_flush/fr/spy"
        args = "/usr/lib/libgtk-3.so.0.2404.19 0x2a8dc0"
        time = 2
    if test == "pp1":
        prog = "attacks/flush_flush/pp/spy"
        args = "/usr/lib/libgtk-3.so.0.2404.19 0x2a8dc0"
        time = 2

    # xlate
    if test == "fr2":
        prog = "attacks/xlate/obj/aes-fr"
        args = "attacks/xlate/openssl-1.0.1e/libcrypto.so.1.0.0"
        time = 2
    if test == "pp2":
        prog = "attacks/xlate/obj/aes-pp"
        args = "attacks/xlate/openssl-1.0.1e/libcrypto.so.1.0.0"
        time = 10
    if test == "xa":
        prog = "attacks/xlate/obj/aes-xa"
        args = "attacks/xlate/openssl-1.0.1e/libcrypto.so.1.0.0"
        time = 10
    if test == "xp":
        prog = "attacks/xlate/obj/aes-xp"
        args = "attacks/xlate/openssl-1.0.1e/libcrypto.so.1.0.0"
        time = 10

    # mastik
    if test == "fr3":
        prog = "attacks/mastik/demo/FR-1-file-access"
        args = "attacks/mastik/demo/FR-1-file-access.c"
        time = 3
    if test == "fr4":
        prog = "attacks/mastik/demo/FR-2-file-access"
        args = "attacks/mastik/demo/FR-1-file-access.c attacks/mastik/demo/FR-2-file-access.c"
        time = 3
    if test == "fr5":
        prog = "attacks/mastik/demo/FR-function-call"
        args = "/usr/lib/libgtk-3.so.0.2404.19 0x2a8dc0"
        time = 3
    if test == "ppl1":
        prog = "attacks/mastik/demo/L1-capture"
        args = "20000"
        time = 5
    if test == "ppl3":
        prog = "attacks/mastik/demo/L3-capture"
        args = "20000"
        time = 5

    if not monitor:
        time = None

    action_exec(prog, args, time, None, monitor)

    if plot:
        tool_plot()


if __name__ == "__main__":

    parser = parser_init()

    args = parser.parse_args()

    if args.module:
        action_module(args.module)
    elif not check_recode():
        print("Recode module not detected... Is it loaded?. Exit")
        exit(0)

    if args.frequency:
        action_frequency(args.frequency)
    
    if args.state:
        action_state(args.state)

    if args.info:
        action_info()

    if args.exec:
        action_exec(args.exec, args.args, args.timeout, args.cpu)

    # print(args)

    if hasattr(args, "attack"):
        tool_test(args.attack, args.tuning, args.monitor, args.plot)

    if hasattr(args, "mitigations"):
        action_mitigations(args.mitigations)

    if args.plot:
        tool_plot()
