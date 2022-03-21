import os
import time
import subprocess
import argparse

HELP_DESC = "Wrapper to attach application execution to ReCoDe profiler"

def parserInit():

    parser = argparse.ArgumentParser(description=HELP_DESC)

    parser.add_argument(
        "program",
        metavar="P",
        type=str,
        nargs="+",
        help="Unload the module",
    )

    parser.add_argument(
        "-l",
        "--loop",
        action="store_true",
        help="After termination, re-exec the process",
    )

    parser.add_argument(
        "-g",
        "--group",
        type=str,
        required=True,
        help="Set the group to attach the program to",
    )

    parser.add_argument(
        "-n",
        "--new",
        action="store_true",
        help="Create a new group instead of using an already present one",
    )

    return parser


def recode_create_group(name):
    RECODE_GROUPS_PATH = "/proc/recode/groups"

    with open (RECODE_GROUPS_PATH + "/create", "w") as file:
        file.write(name)


def recode_register_to_group(name, pid):
    RECODE_GROUPS_PATH = "/proc/recode/groups/" + name + "/processes"

    with open (RECODE_GROUPS_PATH, "w") as file:
        file.write(str(pid))


if __name__ == "__main__":

    parser = parserInit()

    args = parser.parse_args()

    if (args.new):
        recode_create_group(args.group)

    pid = os.getpid()

    recode_register_to_group(args.group, pid)

    print("[WRAPPER] Registered PID %d\n" % (pid));

    os.execvp(args.program[0], args.program)
