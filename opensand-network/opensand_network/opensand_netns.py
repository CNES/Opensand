#!/bin/python3

"""Setup netns for OpenSAND emulator"""


import os
import re
import typing
import argparse
import subprocess

import opensand_network_utils as utils


IDs = typing.Iterable[int]
File = typing.Union[str, typing.IO]


def create_network(tag: str, name: str, address: str, bridge: str):
    veth_remote = f'{tag}_{name}'
    veth_local = f'{tag}_lo_{name}'
    utils.create_veth_ifaces_pair(veth_remote, veth_local)
    utils.move_iface(veth_remote, name)
    utils.add_address(veth_remote, address, name)
    utils.attach_iface(bridge, veth_local)
    utils.set_up(veth_local)
    utils.set_up(veth_remote, name)


def create_entity(entity: str, entity_id: int, dev_bridge: str, emu_bridge: str, create_tap: bool):
    name = f'{entity}{entity_id}'
    utils.create_netns(name)
    utils.set_up('lo', name)
    create_network('dev', name, f'192.168.100.{entity_id}/24', dev_bridge)
    create_network('emu', name, f'192.168.18.{entity_id}/24', emu_bridge)

    if create_tap:
        tap_interface = 'opensand_tap'
        tap_mac = f'00:00:00:00:00:{entity_id:0>2}'
        tap_address = f'192.168.63.{entity_id}/24'
        utils.create_tap_iface(tap_interface, name)
        utils.set_mac_address(tap_interface, tap_mac, name)
        utils.add_address(tap_interface, tap_address, name)
        utils.set_up(tap_interface, name)


def setup(terminals: IDs, gateways: IDs, satellites: IDs, dev_bridge: str, emu_bridge: str):
    utils.create_bridge(dev_bridge, [])
    utils.create_bridge(emu_bridge, [])
    for id in terminals:
        create_entity('st', id, dev_bridge, emu_bridge, True)
    for id in gateways:
        create_entity('gw', id, dev_bridge, emu_bridge, True)
    for id in satellites:
        create_entity('sat', id, dev_bridge, emu_bridge, False)

    utils.add_address(dev_bridge, '192.168.100.100/24')
    utils.set_up(dev_bridge)
    utils.set_up(emu_bridge)


def clean(dev_bridge: str, emu_bridge: str):
    utils.RAISE_ERROR = False

    pattern = re.compile(r'((st|gw|sat)\d+)(\s|$)')
    for netns in utils.list_netns():
        match = pattern.match(netns)
        if match is not None:
            utils.delete_netns(match.group(1))

    utils.delete_iface(dev_bridge)
    utils.delete_iface(emu_bridge)

    utils.RAISE_ERROR = True


def _extract_file_name(filepath: File) -> str:
    try:
        with filepath:
            return filepath.name
    except AttributeError:
        pass

    return filepath


def launch(entity_kind: str, entity_id: int, topology: File, infrastructure: File, profile: File=None):
    topology = _extract_file_name(topology)
    infrastructure = _extract_file_name(infrastructure)
    profile = _extract_file_name(profile)

    command = ['ip', 'netns', 'exec', f'{entity_kind}{entity_id}']

    sudo_user = os.getenv('SUDO_USER')
    if sudo_user:
        command.extend(('sudo', '-E', '-u', sudo_user))

    command.extend(('opensand', '-v', '-i', infrastructure, '-t', topology))
    if profile is not None:
        command.extend(('-p', profile))

    subprocess.run(command, check=True)


def unsigned_byte(value: str) -> int:
    value = int(value)
    if value < 0:
        raise ValueError(f'invalid negative value: {value}')
    if value > 0xFF:
        raise ValueError(f'invalid non-byte value: {value}')
    return value


def command_line_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, fromfile_prefix_chars='@')
    parser.add_argument('-v', '--verbose', action='count', default=0)
    subparsers = parser.add_subparsers(title='Mode', help='Mode of operation')
    subparsers.required = True

    parser_clean = subparsers.add_parser('clean', help='Remove netns that were previously created')
    parser_clean.add_argument(
            '-d', '--dev', '--dev-bridge',
            dest='dev_bridge',
            metavar='NAME',
            default='dev_bridge',
            help='Name of the dev bridge')
    parser_clean.add_argument(
            '-e', '--emu', '--emu-bridge',
            dest='emu_bridge',
            metavar='NAME',
            default='emu_bridge',
            help='Name of the emu bridge')

    parser_setup = subparsers.add_parser('setup', help='Create netns to run OpenSAND in')
    parser_setup.add_argument(
            '-d', '--dev', '--dev-bridge',
            dest='dev_bridge',
            metavar='NAME',
            default='dev_bridge',
            help='Name of the dev bridge')
    parser_setup.add_argument(
            '-e', '--emu', '--emu-bridge',
            dest='emu_bridge',
            metavar='NAME',
            default='emu_bridge',
            help='Name of the emu bridge')
    parser_setup.add_argument(
            '-t', '--st', '--terminal',
            dest='terminals',
            metavar='ID',
            default=(),
            type=unsigned_byte,
            nargs='+',
            help='IDs of terminals to create netns for')
    parser_setup.add_argument(
            '-g', '--gw', '--gateway',
            dest='gateways',
            metavar='ID',
            default=(),
            type=unsigned_byte,
            nargs='+',
            help='IDs of gateways to create netns for')
    parser_setup.add_argument(
            '-s', '--sat', '--satellite',
            dest='satellites',
            metavar='ID',
            default=(),
            type=unsigned_byte,
            nargs='+',
            help='IDs of satellites to create netns for')

    parser_launch = subparsers.add_parser('launch', help='Run OpenSAND inside a netns')
    parser_launch.add_argument(
            'entity_kind',
            choices=('st', 'gw', 'sat'),
            help='Type of the entity that will be launched')
    parser_launch.add_argument(
            'entity_id',
            metavar='ID',
            type=unsigned_byte,
            help='ID of the entity that will be launched')
    parser_launch.add_argument(
            'topology',
            type=argparse.FileType('r'),
            help='Path to the topology.xml file for this entity')
    parser_launch.add_argument(
            'infrastructure',
            type=argparse.FileType('r'),
            help='Path to the infrastructure.xml file for this entity')
    parser_launch.add_argument(
            'profile',
            type=argparse.FileType('r'),
            nargs='?',
            help='Path to the profile.xml file for this entity')

    # Set subparsers options to automatically call the right
    # function depending on the chosen subcommand
    parser_clean.set_defaults(function=clean)
    parser_setup.set_defaults(function=setup)
    parser_launch.set_defaults(function=launch)

    return parser


if __name__ == "__main__":
    args = vars(command_line_parser().parse_args())
    utils.VERBOSITY = args.pop('verbose')
    main = args.pop('function')
    main(**args)
