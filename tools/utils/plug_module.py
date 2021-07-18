from .cmd import Pipe
from .cmd import cmd
import os
from pathlib import Path


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
        action="store_true",
        required=False,
        help="Compile the module",
    )

    plug_parser.add_argument(
        "-cc",
        "--clean-compile",
        action="store_true",
        required=False,
        help="Clean and compile the module",
    )


def __perform_action(action):

    if (action is None or action == ""):
        print("Something wrong: --module illegal")

    out, err, ret = cmd(action, sh=True)

    if ret != 0:
        print("Module action failed with retCode " + str(ret))
        print("Error log: ")
        for line in err.strip().split("\n"):
            print(" \" " + line)
    else:
        print("Done")

    return ret


def action_load():
    action = "make insert"
    print("Mounting module...")
    return __perform_action(action)


def action_unload():
    action = "make remove"
    print("Unmounting module...")
    return __perform_action(action)


def action_load_debug():
    action = "make debug"
    print("Mounting module (Debug ON)...")
    return __perform_action(action)


def action_compile():
    nr_cpu = str(os.cpu_count())
    action = "make -j" + nr_cpu
    print("Compiling module...")
    return __perform_action(action)


def action_clean_compile():
    action = "make clean"
    print("Clean module...")
    __perform_action(action)
    return action_compile()


def validate_args(args):
    if args.command != PLUGIN_NAME:
        return False

    if args.load and args.load_debug:
        print("Cannot execute load twice...")
        return False

    return True


def compute(args):
    if not validate_args(args):
        return False

    # Folder up
    os.chdir(Path(os.getcwd()).parent)

    if (args.unload):
        action_unload()

    if (args.clean_compile):
        action_clean_compile()

    if (args.compile):
        action_compile()

    if (args.load):
        action_load()

    if (args.load_debug):
        action_load_debug()

    return True
