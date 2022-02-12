#!/bin/python3
import argparse
import configparser
from os.path import dirname, abspath

import utils.base.app as app

import sys
sys.path.append(dirname(abspath(__file__)) + "/utils/base")
sys.path.append(dirname(abspath(__file__)) + "/utils/custom")

import plugins.base.plug_module as plug_module
import plugins.base.plug_config as plug_config
import plugins.custom.plug_profiler as plug_profiler
import plugins.custom.plug_data as plug_data
import plugins.custom.plug_network as plug_network
import plugins.custom.plug_security as plug_security
import plugins.custom.plug_scheduler as plug_scheduler
import plugins.custom.plug_stress as plug_stress
import plugins.custom.plug_vm as plug_vm
import plugins.custom.plug_journal_base as plug_journal_base
import plugins.custom.plug_autotest as plug_autotest


PLUGINS = [plug_module, plug_config, plug_profiler, plug_data, plug_network, plug_security, plug_scheduler, plug_stress, plug_vm, plug_journal_base]
# PLUGINS = [plug_module, plug_config, plug_profiler, plug_data, plug_network, plug_security, plug_stress, plug_vm, plug_journal_base]

CONFIG_FILE = 'recode.ini'

class RecodeConfig:
    def __init__(self, file):
        self.file = file
        self.config = None
        self._initConfig()

    def _initConfig(self):
        self.config = configparser.ConfigParser()
        try:
            with open(self.file) as f:
                self.config.read_file(f)
        except IOError:
            None

    def updateConfig(self, key, value):
        self.updateSection(key, value, 'config')

    def readConfig(self, key):
        return self.readSection(key, 'config')

    def updatePath(self, key, value):
        self.updateSection(key, value, 'path')

    def readPath(self, key):
        return self.readSection(key, 'path')

    """ If value is None -> REMOVE, if key exists -> UPDATE, else -> ADD"""
    def updateSection(self, key, value, section):
        if not self.config.has_section(section):
            self.config.add_section(section)

        self.config[section][key] = value
        try:
            with open(CONFIG_FILE, 'w') as f:
                self.config.write(f)
        except IOError:
            print("Error while writing " + self.file)

    def readSection(self, key, section):
        return self.config[section][key]


def fillCommonConfig(config):
    config.updatePath("wd", dirname(abspath(__file__)))
    config.updatePath("accessory", dirname(abspath(__file__)) + "/accessory/obj")
    config.updatePath("recode_proc", "/proc/recode")
    config.updatePath("pmudrv_proc", "/proc/pmudrv")


def parser_init():
    parser = argparse.ArgumentParser(
        description="Tool to interact with Recode Module")

    subparser = parser.add_subparsers(help="commands", dest="command")

    for p in PLUGINS:
        p.setParserArguments(subparser)

    plug_autotest.setParserArguments(subparser)

    return parser


def compute_plugins(args):
    for p in PLUGINS:
        p.compute(args)


if __name__ == "__main__":

    """ TODO Define a class Plugin """
    parser = parser_init()

    args = parser.parse_args()

    app.globalConf = RecodeConfig(CONFIG_FILE)

    fillCommonConfig(app.globalConf)

    cmdList = plug_autotest.compute(args)

    if len(cmdList):
        for cmd in cmdList:
            compute_plugins(parser.parse_args(cmd))
    else:
        compute_plugins(args)
