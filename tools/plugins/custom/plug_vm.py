from os import chdir, cpu_count, strerror, system
from os.path import isfile, dirname, abspath
import subprocess
from shell_cmd import *
from color_printer import *
import random
import time

PLUGIN_NAME = "vm"
HELP_DESC = "Syntactic Sugar to execute available qemu instances"

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
        "-vm",
        "--virtual_machines",
        nargs="+",
        help="Execute the list of vm-instances. each element must be vm:workers",
    )

    plug_parser.add_argument(
        "-w",
        "--workers",
        type=int,
        metavar="N",
        required=False,
        default=1,
        help="Set N workers for the selected vm-instance",
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

    plug_parser.add_argument(
        "-u",
        "--uniform",
        action="store_true",
        help="Use the same worker value for each bench",
    )


usedVMs = [
    # "blackscholes",
    # "canneal",
    "dedup",
    "fluidanimate",
    "freqmine",
    "streamcluster",
    "swaptions",
    "vips",
]
gccVMs = ["swaptions", "vips"]


vmTimeout = {
    # "blackscholes": 16,
    # "canneal": 26,
    "dedup": 3,
    "fluidanimate": 3,
    "freqmine": 6,
    "streamcluster": 3,
    "swaptions": 3,
    "vips": 3,
}


def getAvailableVMs():
    return usedVMs


def check_virtual_machines(vmList):

    if len(vmList) == 0:
        return False

    virtual_machines = getAvailableVMs()
    for sw in vmList:
        if sw.split(":")[0] not in virtual_machines:
            print("Invalid: ", sw.split(":")[0])
            return False

    return True


def get_qemu_cmd(vm, workers):
    # cmd = "qemu-system-x86_64"
    # cmd += " -hda " + WRAPPER_PATH + "/vm/" + vm + ".qcow2"
    # cmd += " -smp " + workers
    # cmd += " --enable-kvm -m 4G"
    # cmd += " -net nic,model=virtio"
    # cmd += " -fsdev local,id=fs1,path=/home/userx,security_model=none"
    # cmd += " -device virtio-9p-pci,fsdev=fs1,mount_tag=host-code"
    # # cmd += " -serial mon:stdio"
    # cmd += " -nographic"

    cmd = "/home/userx/benchmark/parsec3/bin/parsecmgmt"
    cmd += " -a run -p " + vm
    cmd += " -i native -n " + workers

    if vm in gccVMs:
        cmd += " -c gcc-pthreads"

    return cmd.split(" ")


def action_exec(args):
    wrapper = "sponsor"
    if not isfile(wrapper):
        # Compile Wrapper
        cmd("make")

    # _cmd_timeout = ["timeout", "-s", "SIGSTOP"]
    _cmd_wrapper = [WRAPPER_PATH + "/" + wrapper]

    cmd(["python", "tools/recode.py", "module", "-m", "tma_scheduler", "-c", "-l", "-u"])

    processList = []
    for sw in args.virtual_machines:
        stressName, stressWorkers = sw.split(":")
        # processList.append(dcmd(_cmd_wrapper + [stressName] + get_qemu_cmd(stressName, stressWorkers)))
        processList.append(
            dcmd(
                # _cmd_timeout
                # + [str(vmTimeout[stressName])]
                _cmd_wrapper
                + [stressName]
                + get_qemu_cmd(stressName, stressWorkers)
            )
        )
        # print(" ".join(get_qemu_cmd(stressName, stressWorkers)))
        # time.sleep(vmTimeout[stressName])

    dcmd(["python", "tools/recode.py", "config", "-s", "system"])

    for p in processList:
        if p is not None:
            p.wait()

    # dcmd(["python", "tools/recode.py", "config", "-s", "off"])


def action_random_exec(args):

    if args.random == 0:
        return

    if args.workers < args.random:
        wExtra = 0

    vmList = []
    vmWList = []

    wExtra = args.workers - args.random

    virtual_machines = getAvailableVMs()
    args.virtual_machines = []

    if args.random == args.workers and args.random <= len(virtual_machines):
        total = args.random
        while (total > len(virtual_machines)):
            for i in range(len(virtual_machines)):
                args.virtual_machines.append(str(virtual_machines[i] + ":1"))

            total -= len(virtual_machines)

        for i in range(total):
            args.virtual_machines.append(str(virtual_machines[i] + ":1"))
        return

    if args.uniform:
        if args.workers < 1:
            args.workers = 1
        total = args.random
        while (total > len(virtual_machines)):
            for i in range(len(virtual_machines)):
                args.virtual_machines.append(str(virtual_machines[i] + ":" + str(args.workers)))

            total -= len(virtual_machines)

        for i in range(total):
            args.virtual_machines.append(str(virtual_machines[i] + ":" + str(args.workers)))
        return

    for i in range(args.random):
        vmList.append(random.choice(virtual_machines))
        vmWList.append(1)

    while wExtra > 0:
        for i in range(len(vmWList)):
            if wExtra < 1:
                break
            eW = random.randint(1, int(max(wExtra, cpu_count()) / 2))
            vmWList[i] += eW
            wExtra -= eW

    for i in range(len(vmWList)):
        args.virtual_machines.append(vmList[i] + ":" + str(vmWList[i]))


def validate_args(args):
    return args.command == PLUGIN_NAME


def compute(args, config):
    if not validate_args(args):
        return False

    chdir(WD_PATH)

    action_random_exec(args)

    print(args.virtual_machines)

    if not check_virtual_machines(args.virtual_machines):
        return True

    action_exec(args)

    return True
