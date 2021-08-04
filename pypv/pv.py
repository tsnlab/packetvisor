import os
import re
import shlex
import shutil
import signal
import subprocess
import sys
import tempfile

from contextlib import ExitStack
from collections import namedtuple
from typing import Dict, List

from config import convert

import yaml

DRIVER_NAME = 'uio_pci_generic'
HUGE_PAGE_SIZE = 2 * 1024 * 1024  # 2MiB
TTY_WIDTH = shutil.get_terminal_size((80, 24)).columns

DevInfo = namedtuple('DevInfo', ('drv', 'pci'))


def run_cmd(cmd: str) -> str:
    return subprocess.check_output(shlex.split(cmd)).decode('utf-8')


def get_nic_info() -> Dict[str, DevInfo]:
    pattern = re.compile(r"^(?P<pci>[\d:.]+) '[^']+' (?:if=(?P<ifname>\S+) )?drv=(?P<drv>\S+).*$", re.MULTILINE)

    device_info = run_cmd("dpdk-devbind.py --status-dev net")
    return {
        m.group('ifname'): DevInfo(drv=m.group('drv'), pci=m.group('pci'))
        for m in pattern.finditer(device_info)
        if m.group('ifname') is not None
    }


def calc_hugemem_size(config: Dict) -> int:
    size = (
        config['memory']['shared_memory'] +
        350_000 +  # Some constant usage from mbuf_pool
        (
            (config['memory']['packet_pool'] * len(config['nics'])) +
            sum(
                nic['rx_queue'] + nic['tx_queue']
                for nic in config['nics']
            )
        ) * 2_048
    )

    remainder = size % HUGE_PAGE_SIZE
    if remainder > 0:
        size = size - remainder + HUGE_PAGE_SIZE

    return size


def main(args: List[str]) -> int:
    args.pop(0)  # __main__

    if len(args) < 1:
        print('Need app root. config.yaml must be in app root')
        return 1

    app_path = os.path.abspath(args.pop(0))
    config_path = os.path.join(os.path.dirname(app_path), 'config.yaml')

    with open(config_path) as f:
        config = yaml.load(f, Loader=yaml.FullLoader)

    required_hugemem_size = calc_hugemem_size(config)

    print(f'Required hugemem size: {required_hugemem_size:,}B')

    config['eal_params'] = []
    eal_params = config['eal_params']

    try:
        cores = config.get('cores', 0)
        if len(cores) == 0:
            raise ValueError('Invalid core count')

        eal_params += ['-l', ','.join(str(x) for x in cores)]
    except Exception as e:
        print(f'Invalid cores config {e}')
        return 1

    system_nics = get_nic_info()

    for index, nic in enumerate(config.get('nics')):

        dev = nic.get('dev')
        if not dev:
            print(f'Failed to parse /nics[{index}]/dev')
            return 1

        devinfo: DevInfo = system_nics.get(nic['dev'])
        if not devinfo:
            print(f"No pci has dev name '{nic['dev']}'")
            return 1

        nic['drv'] = devinfo.drv
        nic['dev'] = devinfo.pci

    def cleanup():
        print('\n' + '=' * TTY_WIDTH)
        print('Cleaning up')

        for nic in config['nics']:
            run_cmd(f"dpdk-devbind.py -b {nic['drv']} {nic['dev']}")
        run_cmd('dpdk-hugepages.py -c')
        run_cmd('dpdk-hugepages.py -u')

    with ExitStack() as estack:
        estack.callback(cleanup)

        print('Setup hugepages')
        try:
            run_cmd(f"dpdk-hugepages.py -p {HUGE_PAGE_SIZE} --setup {required_hugemem_size}")
        except Exception as e:
            print(f'Failed to setup hugepage: {e}')
            return 1

        print('Load module')
        run_cmd(f'modprobe {DRIVER_NAME}')

        print('Bind nic')
        for nic in config['nics']:
            run_cmd(f"dpdk-devbind.py -b {DRIVER_NAME} {nic['dev']}")

        app_env = os.environ.copy()

        # Setup config
        with tempfile.NamedTemporaryFile(mode='w', delete=False) as config_file:
            print(f'TMP Config using {config_file.name}')
            estack.callback(lambda: os.unlink(config_file.name))
            config_file.write(convert(config))
            app_env['PV_CONFIG'] = config_file.name

        print('Starting app')
        print('\n' + '=' * TTY_WIDTH)

        proc = subprocess.Popen([app_path, *args], start_new_session=True, env=app_env)

        def handler(signum, frame):
            proc.send_signal(signal.SIGINT)

        default_handler = signal.signal(signal.SIGINT, handler)
        exitcode = proc.wait()
        signal.signal(signal.SIGINT, default_handler)
        return exitcode


if __name__ == '__main__':
    sys.exit(main(sys.argv))
