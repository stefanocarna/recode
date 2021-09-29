from .printer import *


PLUGIN_NAME = "autotest"
HELP_DESC = "Configure Recode to execute test with minimum effort"


RECODE_PROC_PATH = "/proc/recode"
DEFAULT_DATA_FILE = "data.json"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    plug_parser.add_argument(
        "-x",
        "--exec",
        metavar="prog",
        type=str,
        required=True,
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

    plug_parser.add_argument(
        "-f",
        "--file",
        metavar="F",
        nargs='?',
        type=str,
        required=True,
        const=DEFAULT_DATA_FILE,
        help="Specify the file's name by (F) for the TMA results.",
    )


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args, config):
    cmdList = []

    if not validate_args(args):
        return cmdList

    if not args.exec:
        return cmdList

    cmdList.append("module -u -c -l".split())
    cmdList.append("config -f 30".split())
    cmdList.append("config -s system".split())

    execStr = ("profiler -x " + str(args.exec)).split()
    if args.args is not None:
        execStr.append("-a")
        execStr.append(str(args.args))

    if args.timeout is not None:
        execStr.append("-t")
        execStr.append(str(args.timeout))

    print("args", str(execStr))

    cmdList.append(execStr)
    cmdList.append("config -s off".split())

    if args.file is not None:
        cmdList.append(("data -etma " + args.file).split())

    # cmdList.append(("network -l -s " + args.file).split())

    return cmdList
