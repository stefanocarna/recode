import os
from .cmd import cmd
from .printer import *

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


def action_exec(prog, args, timeout, cpu, profile=True):
    wrapper = "wrapper"
    if not os.path.isfile(wrapper):
        # Compile Wrapper
        cmd("make")

    if timeout is not None and timeout < 0:
        pr_warn("Invalid timeout (< 0). Ignore timeout")
        timeout = None

    _cmd = ["./" + wrapper] if profile else []

    if cpu is not None:
        if cpu < 0 or cpu >= os.cpu_count():
            pr_warn("Invalid cpu " + str(cpu) + ". Ignore cpu mask")

        else:
            _cmd = ["taskset", "-c", str(cpu)] + _cmd

    _cmd = _cmd + [prog]

    if args is not None:
        _cmd = _cmd + args.split()

    pr_info("Exec: " + str(_cmd) + " @ " + str(timeout))
    out, err, ret = cmd(_cmd, timeOut=timeout)

    if ret != 0:
        pr_err("Execution failed with errcode " + str(ret) + ":")
        pr_warn(" * " + str(err))
    else:
        pr_text("OUT pipe: " + out)


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args):
    if not validate_args(args):
        return False

    if args.exec:
        action_exec(args.exec, args.args, args.timeout, args.cpu)

    return True
