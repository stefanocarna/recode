from utils.base import color_printer as ep
from utils.base import app


PLUGIN_NAME = "config"
HELP_DESC = "Configure Recode Module's paramenters"


def setParserArguments(parser):

    plug_parser = parser.add_parser(PLUGIN_NAME, help=HELP_DESC)

    plug_parser.add_argument(
        "-s",
        "--state",
        type=str,
        required=False,
        choices=["off", "tuning", "profile", "system", "idle", "reset", "kill"],
        help="Set the Recode state: (on) enabled, (off) disabled, (reset) off -> on",
    )

    plug_parser.add_argument(
        "-tt",
        "--tuning-type",
        type=str,
        required=False,
        metavar="T",
        choices=["FR", "XL"],
        help="Set the Recode tuning type: T",
    )

    plug_parser.add_argument(
        "-tma-m",
        "--tma-mode",
        type=str,
        required=False,
        choices=["fix", "dyn", "off"],
        help="Enable or disable TMA",
    )

    plug_parser.add_argument(
        "-tma-l",
        "--tma-level",
        type=str,
        required=False,
        help="Set the maximum TMA level",
    )

    plug_parser.add_argument(
        "-sc",
        "--scheduler",
        type=str,
        required=False,
        choices=["direct", "dynamic", "collect", "off"],
        help="Enable or disable TMA",
    )

    plug_parser.add_argument(
        "-f",
        "--frequency",
        metavar="F",
        type=int,
        required=False,
        help="Set the Recode profiling (F)requency",
    )

    plug_parser.add_argument(
        "-i",
        "--info",
        action="store_true",
        required=False,
        help="Get current Recode configuration",
    )

    """ Enabled only with security module """
    plug_parser.add_argument(
        "-m",
        "--mitigations",
        type=str,
        nargs="+",
        metavar="CONF",
        choices=["te", "exile", "llc", "verbose", "none"],
        required=False,
        help="Configure the module",
    )


def action_state(action):
    path = app.globalConf.readPath("recode_proc") + "/state"

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
    elif action == "kill":
        value = "5"
    elif action == "reset":
        action_state("off")
        value = "2"
    else:
        ep.pr_warn("Something wrong: --state " + action + " illegal")
        return

    _file = open(path, "w")
    _file.write(value)
    _file.close()


def action_tma_mode(action):
    path = app.globalConf.readPath("pmudrv_proc") + "/tma_mode"

    if action == "off":
        value = "0"
    elif action == "fix":
        value = "1"
    elif action == "dyn":
        value = "2"
    else:
        ep.pr_warn("Something wrong: --tma " + action + " illegal")
        return

    _file = open(path, "w")
    _file.write(value)
    _file.close()


def action_tma_level(action):
    path = app.globalConf.readPath("pmudrv_proc") + "/tma_level"

	# Should check the compile time value

    _file = open(path, "w")
    _file.write(action)
    _file.close()


def action_scheduler(action):
    path = app.globalConf.readPath("recode_proc") + "/scheduler"

    if action == "off":
        value = "0"
    elif action == "collect":
        value = "1"
    elif action == "direct":
        value = "2"
    elif action == "dynamic":
        value = "3"
    else:
        ep.pr_warn("Something wrong: --scheduler " + action + " illegal")
        return

    _file = open(path, "w")
    _file.write(value)
    _file.close()


def action_frequency(value):
    value = int(value)
    if value < 1 and value > 48:
        ep.pr_warn("frequency must be expressed as (1 << P) + 1")
        return

    pmc_mask = (1 << value) - 1

    path = app.globalConf.readPath("pmudrv_proc") + "/frequency"

    _file = open(path, "w")

    _file.write(hex(pmc_mask))
    _file.close()


def get_info(info):
    if info not in ["frequency", "events", "thresholds"]:
        ep.pr_warn("Cannot read " + info + " info. Returning 0")
        return 0

    path = app.globalConf.readPath("pmudrv_proc") + "/" + info
    _file = open(path, "r")
    value = _file.readlines()
    _file.close()
    return " ".join([str(e) for e in value]).strip()


def action_info():
    ep.pr_info("Recode info:")
    ep.pr_text(" - frequency:\t" + hex(int("0x" + get_info("frequency"), 0)))
    ep.pr_text(" - events:\t" + get_info("events"))
    ep.pr_text(" - thresholds:\t" + get_info("thresholds"))


def action_mitigations(mitigations):

    path = app.globalConf.readPath("recode_proc") + "/mitigations"

    _file = open(path, "w")

    mask = 0

    # define DM_G_LLC_FLUSH			0
    # define DM_G_CPU_EXILE			1
    # define DM_G_TE_MITIGATE		2
    # define DM_G_VERBOSE			18

    if "llc" in mitigations:
        mask |= 1 << 0
    if "exile" in mitigations:
        mask |= 1 << 1
    if "te" in mitigations:
        mask |= 1 << 2
    if "verbose" in mitigations:
        mask |= 1 << 18
    if "none" in mitigations:
        mask = 0

    print(hex(mask))

    _file.write(str(mask))
    _file.flush()
    _file.close()


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args):
    if not validate_args(args):
        return False

    if args.frequency:
        action_frequency(args.frequency)

    if args.tma_mode:
        action_tma_mode(args.tma_mode)

    if args.tma_level:
        action_tma_level(args.tma_level)

    if args.scheduler:
        action_scheduler(args.scheduler)

    if args.state:
        action_state(args.state)
        # action_state(args.state, args.tuning_type)

    if args.info:
        action_info()

    if args.mitigations:
        action_mitigations(args.mitigations)

    return True
