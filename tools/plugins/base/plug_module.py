import os
from os.path import dirname, abspath
from shell_cmd import *
from color_printer import *
from pathlib import Path
import sys
sys.path.append("...")
import utils.base.app as app

PLUGIN_NAME = "module"
HELP_DESC = "Basic interface to manage the Recode Module"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    plug_parser.add_argument(
        "-l",
        "--load",
        action="store_true",
        required=False,
        help="Load the module",
    )

    plug_parser.add_argument(
        "-u",
        "--unload",
        action="store_true",
        required=False,
        help="Unload the module",
    )

    plug_parser.add_argument(
        "-ld",
        "--load-debug",
        action="store_true",
        required=False,
        help="Load the module with debug facilities",
    )

    plug_parser.add_argument(
        "-c",
        "--compile",
        metavar="P",
        nargs='?',
        type=str,
        required=False,
        help="Compile the module",
    )

    plug_parser.add_argument(
        "-cc",
        "--clean-compile",
        metavar="P",
        nargs='?',
        type=str,
        required=False,
        help="Clean and compile the module",
    )


def __perform_action(action):

    if (action is None or action == ""):
        pr_warn("Something wrong: --module illegal")
        return

    pr_info(action)
    out, err, ret = cmd(action, sh=True)

    if ret != 0:
        pr_err("Module action failed with retCode " + str(ret) + ":")
        for line in err.strip().split("\n"):
            pr_warn(" * " + line)
    else:
        pr_succ("Done")

    return ret


def action_load():
    action = "make load"
    pr_info("Mounting module...")
    return __perform_action(action)


def action_unload():
    action = "make unload"
    pr_info("Unmounting module...")
    return __perform_action(action)


def action_load_debug():
    action = "make debug"
    pr_info("Mounting module (Debug ON)...")
    return __perform_action(action)


def action_compile(args):
    nr_cpu = str(os.cpu_count())
    action = "make " + "POP_MODULE=" + args + " -j" + nr_cpu
    pr_info("Compiling module...")
    return __perform_action(action)


def action_clean_compile(args):
    action = "make clean"
    pr_info("Clean module...")
    __perform_action(action)
    return action_compile(args)


def validate_args(args):
    if args.command != PLUGIN_NAME:
        return False

    if args.load and args.load_debug:
        pr_warn("Cannot execute load twice...")
        return False

    return True


def compute(args):
    if not validate_args(args):
        return False

    os.chdir(app.globalConf.readPath("wd"))

    if (args.unload):
        action_unload()

    if (args.clean_compile):
        action_clean_compile(args.clean_compile)

    if (args.compile):
        action_compile(args.compile)

    if (args.load):
        action_load()

    if (args.load_debug):
        action_load_debug()

    return True
