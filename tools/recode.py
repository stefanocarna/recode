#!/bin/python3
import argparse
# from recode import plot

import utils.plug_autotest as plug_autotest
import utils.plug_module as plug_module
import utils.plug_config as plug_config
import utils.plug_profiler as plug_profiler
import utils.plug_data as plug_data


def parser_init():
    parser = argparse.ArgumentParser(
        description="Tool to interact with Recode Module")

    subparser = parser.add_subparsers(help="commands", dest="command")

    plug_autotest.setParserArguments(subparser)
    plug_module.setParserArguments(subparser)
    plug_config.setParserArguments(subparser)
    plug_profiler.setParserArguments(subparser)
    plug_data.setParserArguments(subparser)

    return parser


def compute_plugins(args):
    plug_module.compute(args)
    plug_config.compute(args)
    plug_profiler.compute(args)
    plug_data.compute(args)


if __name__ == "__main__":

    parser = parser_init()

    args = parser.parse_args()

    # if not check_recode():
    #     print("Recode module not detected... Is it loaded?. Exit")
    #     exit(0)

    cmdList = plug_autotest.compute(args)

    if len(cmdList):
        for cmd in cmdList:
            compute_plugins(parser.parse_args(cmd))
    else:
        compute_plugins(args)
