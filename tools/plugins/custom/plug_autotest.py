import os
import sys
sys.path.append("...")
import utils.base.app as app

PLUGIN_NAME = "autotest"
HELP_DESC = "Configure Recode to execute test with minimum effort"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

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
        "-f",
        "--file",
        metavar="F",
        type=str,
        required=False,
        help="Save the results into F",
    )

    plug_parser.add_argument(
        "-c",
        "--cpu",
        type=int,
        required=False,
        help="Bind the program execution to a specific cpu",
    )


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args):
    cmdList = []

    if not validate_args(args):
        return cmdList

    ss = ["matrix", "cpu", "io", "matrix-3d", "cache", "bsearch", "qsort", "hdd", "fork", "bigheap"]

    for s in ss:
        cmdList.append(("stress -s " + s + ":1").split())
        cmdList.append(("stress -s " + s + ":" + str(os.cpu_count())).split())

    for i in range(1, 3):
        for k in [1, 2, 4, 8, 16]:
            base = ""
            for s in ss:
                for j in range(i):
                    base += " " + s + ":" + str(k)
            cmdList.append(("stress -s" + base).split())
            cmdList.append(("scheduler -n").split() + [base])

    print(cmdList)
    return cmdList
