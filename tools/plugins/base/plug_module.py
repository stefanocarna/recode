import os
from utils.base import cmd
from utils.base import color_printer as ep
from utils.base import app


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

    plug_parser.add_argument(
        "-p",
        "--plugin",
        type=str,
        required=False,
        help="Compile the module",
    )


def __perform_action(action):

    if action is None or action == "":
        ep.pr_warn("Something wrong: --module illegal")
        return

    ep.pr_info(action)
    out, err, ret = cmd.cmd(action, sh=True)

    if ret != 0:
        ep.pr_err("Module action failed with retCode " + str(ret) + ":")
        for line in err.strip().split("\n"):
            ep.pr_warn(" * " + line)
    else:
        ep.pr_succ("Done")

    return ret


def action_load():
    action = "make load"
    ep.pr_info("Mounting module...")
    return __perform_action(action)


def action_unload():
    action = "make unload"
    ep.pr_info("Unmounting module...")
    return __perform_action(action)


def action_load_debug():
    action = "make debug"
    ep.pr_info("Mounting module (Debug ON)...")
    return __perform_action(action)


def action_compile(plugin=None):
    nr_cpu = str(os.cpu_count())
    if plugin != None:
        action = "make " + "POP_MODULE=" + plugin + " -j" + nr_cpu
    else:
        action = "make -j" + nr_cpu

    ep.pr_info("Compiling module...")
    return __perform_action(action)


def action_clean_compile(plugin):
    action = "make clean"
    ep.pr_info("Clean module...")
    __perform_action(action)
    return action_compile(plugin)


def validate_args(args):
    if args.command != PLUGIN_NAME:
        return False

    if args.load and args.load_debug:
        ep.pr_warn("Cannot execute load twice...")
        return False

    return True


def compute(args):
    if not validate_args(args):
        return False

    os.chdir(app.globalConf.readPath("wd"))

    ep.pr_info(app.globalConf.readPath("wd"))

    if args.unload:
        action_unload()

    if args.clean_compile:
        action_clean_compile(args.plugin)

    if args.compile:
        action_compile(args.plugin)

    if args.load:
        action_load()

    if args.load_debug:
        action_load_debug()

    return True
