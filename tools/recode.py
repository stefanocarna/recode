#!/bin/python3

import argparse
# from recode import plot

import utils.plug_module as plug_module
import utils.plug_config as plug_config
import utils.plug_profiler as plug_profiler


def parser_init():
    parser = argparse.ArgumentParser(
        description="Tool to interact with Recode Module")

    subparser = parser.add_subparsers(help="commands", dest="command")

    plug_module.setParserArguments(subparser)
    plug_config.setParserArguments(subparser)
    plug_profiler.setParserArguments(subparser)

    return parser


if __name__ == "__main__":

    parser = parser_init()

    args = parser.parse_args()

    # if not check_recode():
    #     print("Recode module not detected... Is it loaded?. Exit")
    #     exit(0)

    plug_module.compute(args)
    plug_config.compute(args)
    plug_profiler.compute(args)

    # if args.plot:
    #     tool_plot()
