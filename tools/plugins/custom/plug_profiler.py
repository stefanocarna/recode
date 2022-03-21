from os import chdir, cpu_count
from os.path import isfile, dirname, abspath

import sys
sys.path.append("...")

import utils.base.app as app
from utils.base.shell_cmd import *
from utils.base.color_printer import *
from plugins.base.plug_config import *


PLUGIN_NAME = "profiler"
HELP_DESC = "Configure profiling activity"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    # parser.add_argument('-x', '--exec', metavar='prog', nargs='+', required=False,
    # 	help="Execute and profile a given program. Specific program arguments inline")

    plug_parser.add_argument(
        "-x",
        "--exec",
        metavar="prog",
        type=str,
        required=False,
        help="Execute and profile a given program. Specific program arguments inline",
    )

    plug_parser.add_argument(
        "-a",
        "--args",
        type=str,
        required=False,
        help="Execute and profile a given program. Specific program arguments inline",
    )

    plug_parser.add_argument(
        "-gn",
        "--group-name",
        type=str,
        required=False,
        help="Specify the group name seen by the profiler driver",
    )

    plug_parser.add_argument(
        "-t",
        "--timeout",
        metavar="T",
        type=int,
        required=False,
        help="Set an execution timeout",
    )

    plug_parser.add_argument(
        "-c",
        "--cpu",
        type=int,
        required=False,
        help="Bind the program execution to a specific cpu",
    )

    plug_parser.add_argument(
        "-sw",
        "--system-wide",
        action="store_true",
        required=False,
        help="Bind the program execution to a specific cpu",
    )


def action_exec(args, profile=True):

    prog = args.exec
    cpu = args.cpu
    timeout = args.timeout
    system_wide = args.system_wide
    prog_args = args.args
    group_name = args.group_name

    wrapper = "wrapper"
    if not isfile(wrapper):
        # Compile Wrappersystem_w
        cmd("make")

    if timeout is not None and timeout < 0:
        ep.pr_warn("Invalid timeout (< 0). Ignore timeout")
        timeout = None

    _cmd = [app.globalConf.readPath("accessory") + "/" + wrapper] if profile else []

    if cpu is not None:
        if cpu < 0 or cpu >= cpu_count():
            ep.pr_warn("Invalid cpu " + str(cpu) + ". Ignore cpu mask")

        else:
            _cmd = ["taskset", "-c", str(cpu)] + _cmd

    if group_name is not None:
        _cmd = _cmd + [group_name] + [prog]
    else:
        _cmd = _cmd + [prog] + [prog]

    if prog_args is not None:
        _cmd = _cmd + prog_args.split()

    if system_wide:
        action_state("system")

        ep.pr_info("[SYSTME_WIDE] Exec: " + str(_cmd))
        p = dcmd(_cmd)
        p.wait()

        action_state("off")

    else:
        ep.pr_info("Exec: " + str(_cmd) + " @ " + str(timeout))
        out, err, ret = cmd(_cmd, timeOut=timeout)

        if ret != 0:
            ep.pr_err("Execution failed with errcode " + str(ret) + ":")
            ep.pr_warn(" * " + str(err))
        else:
            ep.pr_text("OUT pipe: " + out)
            ep.pr_text("ERR pipe: " + err)


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args):
    if not validate_args(args):
        return False

    chdir(app.globalConf.readPath("tools"))

    if args.exec:
        action_exec(args)

    return True
