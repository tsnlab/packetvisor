#!/bin/python3
import subprocess
import pprint
import yaml
import sys
import os
import signal

# ------ User config starts ------

PMD = "uio_pci_generic"
HUGE_PAGE_SIZE = "2M"
HUGE_TOTAL_SIZE = "4G"

# External libraries for network and memory
external_libs = ['/usr/local/lib/x86_64-linux-gnu/librte_net_e1000.so', '/usr/local/lib/x86_64-linux-gnu/librte_mempool_ring.so']

# ------ User config ends ------

# The PCI base class for all devices
network_class = {'Class': '02', 'Vendor': None, 'Device': None, 'SVendor': None, 'SDevice': None}
ifpga_class = {'Class': '12', 'Vendor': '8086', 'Device': '0b30', 'SVendor': None, 'SDevice': None}
cavium_pkx = {'Class': '08', 'Vendor': '177d', 'Device': 'a0dd,a049', 'SVendor': None, 'SDevice': None}
avp_vnic = {'Class': '05', 'Vendor': '1af4', 'Device': '1110', 'SVendor': None, 'SDevice': None}

network_devices = [network_class, cavium_pkx, avp_vnic, ifpga_class]

def device_type_match(dev, devices_type):
    for i in range(len(devices_type)):
        param_count = len(
            [x for x in devices_type[i].values() if x is not None])
        match_count = 0
        if dev["Class"][0:2] == devices_type[i]["Class"]:
            match_count = match_count + 1
            for key in devices_type[i].keys():
                if key != 'Class' and devices_type[i][key]:
                    value_list = devices_type[i][key].split(',')
                    for value in value_list:
                        if value.strip(' ') == dev[key]:
                            match_count = match_count + 1
            # count must be the number of non None parameters to match
            if match_count == param_count:
                return True
    return False


def get_pci_addr_by_dev(dev_name):
    dev_lines = subprocess.check_output(["lshw", "-class", "network", "-businfo"]).splitlines()
    for dev_line in dev_lines:
        info_list = dev_line.decode("utf8").split()
        if "pci" not in info_list[0]:
            continue

        if dev_name == info_list[1]:
            return info_list[0][4:]

    return None


def get_pci_devices():
    devices = {}
    devices_type = network_devices
    dev = {}
    dev_lines = subprocess.check_output(["lspci", "-Dvmmnnk"]).splitlines()
    for dev_line in dev_lines:
        if not dev_line:
            if device_type_match(dev, devices_type):
                # Replace "Driver" with "Driver_str" to have consistency of
                # of dictionary key names
                if "Driver" in dev.keys():
                    dev["Driver_str"] = dev.pop("Driver")
                if "Module" in dev.keys():
                    dev["Module_str"] = dev.pop("Module")
                # use dict to make copy of dev
                devices[dev["Slot"]] = dict(dev)
            # Clear previous device's data
            dev = {}
        else:
            name, value = dev_line.decode("utf8").split("\t", 1)
            value_list = value.rsplit(' ', 1)
            if value_list:
                # String stored in <name>_str
                dev[name.rstrip(":") + '_str'] = value_list[0]
            # Numeric IDs
            dev[name.rstrip(":")] = value_list[len(value_list) - 1] \
                .rstrip("]").lstrip("[")

    return devices

def main(arg):
    if len(arg) == 1:
        print("Need root. config.yaml MUST be in app's path")
        print("Usage: ", os.path.basename(arg[0]), " [app]")
        sys.exit(0)

    app_path = arg[1] if os.path.isabs(arg[1]) else os.path.abspath(arg[1])
    config_path = os.path.join(os.path.dirname(app_path), 'config.yaml')

    with open(config_path, 'r') as f:
        config = yaml.load(f, Loader=yaml.FullLoader)

    config['eal_params'] = []
    eal_params = config['eal_params']

    # parse cores
    if config.get('cores') is None:
        print("Failed to parse '/cores' from config")
        sys.exit(0)

    if len(config.get('cores')) == 0:
        print("Invalid cores count: ", len(config.get('cores')))
        sys.exit(0)

    eal_params.append('-l')
    eal_params.append(','.join(str(x) for x in config.get('cores')))

    # external libs
    for ext_lib in external_libs:
        eal_params.append('-d')
        eal_params.append(ext_lib)

    # parse nics
    devices = get_pci_devices()

    if config.get('nics') is None:
        print("Failed to parse '/nics' from config")
        sys.exit(0)

    for idx, nic in enumerate(config.get('nics')):
        if nic.get('dev') is None:
            print("Failed to parse '/nics[", idx, "]/dev")
            sys.exit(0)

        pci_addr = get_pci_addr_by_dev(nic.get('dev'))
        if pci_addr is None:
            print("No pci has dev name '" + nic.get('dev') + "'")
            sys.exit(0)

        nic['module'] = devices[pci_addr]['Module_str']
        nic['dev'] = pci_addr

    global HUGE_PAGE_SIZE, HUGE_TOTAL_SIZE, PMG
    # mount and reserve hugepage
    print("PV: setup hugepages...")
    subprocess.call(["sudo", "dpdk-hugepages.py", "-p", str(HUGE_PAGE_SIZE), "--setup", str(HUGE_TOTAL_SIZE)])

    # bind nics
    print("PV: load '" + PMD + "' module...")
    subprocess.call(["sudo", "modprobe", PMD])

    print("PV: bind NIC...")
    for nic in config.get('nics'):
        subprocess.call(["sudo", "dpdk-devbind.py", "-b=" + PMD, nic['dev']])

    # the tmp_config created here will be parsed from app's pv_init()
    tmp_config_path = '.tmp_' + os.path.basename(config_path)
    with open(tmp_config_path, 'w') as f:
        yaml.dump(config, f, default_flow_style=True)

    # start app
    print("PV: start app...")
    print("\n------------------------------------------------------------\n")
    proc = subprocess.Popen(["sudo", "-S", app_path], start_new_session=True)

    def handler(signum, frame):
        proc.send_signal(signum)

    default_handler = signal.signal(signal.SIGINT, handler)
    proc.wait()
    signal.signal(signal.SIGINT, default_handler)

    # remove tmp_config
    os.remove(tmp_config_path)

    # unbind nics
    print("\n------------------------------------------------------------\n")
    print("PV: unbind NIC...")
    for nic in config.get('nics'):
        subprocess.call(["sudo", "dpdk-devbind.py", "-b=" + nic['module'], nic['dev']])

    # clear unmount hugepage
    print("PV: clear hugepage...")
    subprocess.call(["sudo", "dpdk-hugepages.py", "-c"])
    subprocess.call(["sudo", "dpdk-hugepages.py", "-u"])

if __name__ == "__main__":
    main(sys.argv)
