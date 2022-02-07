from os import chdir, cpu_count, strerror, system
from os.path import isfile, dirname, abspath
import subprocess
from .cmd from shell_cmd import *, dcmd
from .printer import *
import random

PLUGIN_NAME = "stress"
HELP_DESC = "Syntactic Sugar to execute stress-ng suite"

WD_PATH = dirname(dirname(dirname(abspath(__file__))))
WRAPPER_PATH = dirname(dirname(abspath(__file__)))


def init(wd_path):
    global WD_PATH 
    WD_PATH = wd_path


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    # parser.add_argument('-x', '--exec', metavar='prog', nargs='+', required=False,
    # 	help="Execute and profile a given program. Specific program arguments inline")

    plug_parser.add_argument(
        "-s",
        "--stressors",
        nargs='+',
        help="Execute the list of stressor. each element must be stressor:workers",
    )

    plug_parser.add_argument(
        "-w",
        "--workers",
        type=int,
        metavar="N",
        required=False,
        default=1,
        help="Set N workers for the selected stressor",
    )

    plug_parser.add_argument(
        "-t",
        "--timeout",
        metavar="T",
        type=int,
        required=False,
        help="Set an execution timeout (T)",
    )

    plug_parser.add_argument(
        "-c",
        "--cpu",
        type=int,
        required=False,
        help="Bind the program execution to a specific cpu",
    )

    plug_parser.add_argument(
        "-r",
        "--random",
        metavar="R",
        type=int,
        required=False,
        default=0,
        help="Run R random tests, if w is specified, also W is randomly split for each test",
    )


usedStressors = ["matrix", "cpu", "io", "matrix-3d", "cache", "bsearch"]


def getAvailableStressors():
    return usedStressors

    out, err, ret = cmd(["stress-ng", "--stressor"])

    if ret != 0:
        pr_err("Execution failed with errcode " + str(ret) + ":")
        pr_warn(" * " + str(err))
        return []

    return out.split(" ")


def check_stressors(stressorList):

    if len(stressorList) == 0:
        return False

    stressors = getAvailableStressors()
    for sw in stressorList:
        if sw.split(":")[0] not in stressors:
            return False

    return True


def action_exec(args):
    wrapper = "wrapper"
    if not isfile(wrapper):
        # Compile Wrapper
        cmd("make")

    _cmd_wrapper = [WRAPPER_PATH + "/" + wrapper]
    _cmd_stress = ["stress-ng"]

    if args.timeout is not None and args.timeout > 0:
        _cmd_stress =_cmd_stress + ["--timeout", str(args.timeout)]

    if args.cpu is not None and (args.cpu > 0 or args.cpu <= cpu_count()):
        _cmd_stress =_cmd_stress + ["--taskset", str(args.cpu)]

    if args.workers < 1:
        args.workers = 1

    processList = []
    for sw in args.stressors:
        stressName, stressWorkers = sw.split(":")
        processList.append(dcmd(_cmd_wrapper + [stressName] + _cmd_stress + ["--" + stressName, stressWorkers]))

    dcmd(["python", "tools/recode.py", "config", "-s", "system"])

    for p in processList:
        if p is not None:
            p.wait()

    # cmd(["python", "tools/recode.py", "scheduler", "-pc"])
    dcmd(["python", "tools/recode.py", "config", "-s", "off"])

    # pr_info("Cwd: " + str(WD_PATH))
    # pr_info("Exec: " + str(_cmd) + " @ " + str(args.timeout))
    # cmd(_cmd)


def action_random_exec(args):

    if args.random == 0:
        return

    if args.workers < args.random:
        wExtra = 0

    stressorList = []
    stressorWList = []

    wExtra = args.workers - args.random

    stressors = getAvailableStressors()
    args.stressors = []

    for i in range(args.random):
        stressorList.append(random.choice(stressors))
        stressorWList.append(1)

    while wExtra > 0:
        for i in range(len(stressorWList)):
            if (wExtra < 1):
                break
            eW = random.randint(1, int(max(wExtra, cpu_count()) / 2))
            stressorWList[i] += eW
            wExtra -= eW

    for i in range(len(stressorWList)):
        args.stressors.append(stressorList[i] + ":" + str(stressorWList[i]))


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args, config):
    if not validate_args(args):
        return False

    chdir(WD_PATH)

    action_random_exec(args)

    print(args.stressors)

    if not check_stressors(args.stressors):
        return True

    action_exec(args)

    return True
