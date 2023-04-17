# Packetvisor 3.0

## Prerequisite

### bpftool & XDP-tools (submodules)
PV 3.0 mainly uses XDP-tools lib and bpftool is used for compiling XDP-tools lib.

The script, `build.sh`, includes submodule update so that you can do the next step.

## Build
You can build PV 3.0 by executing `build.sh`

`sudo sh build.sh`

## Use
Superuser is needed to execute PV 3.0.

The following is the usage of PV 3.0.

`sudo ./pv <inferface name> <chunk size> <chunk count> <Filling ring size> <RX ring size> <Completion ring size> <TX ring size>`

## Run examples
You can use `set_veth.sh` to set veths for testing the application.
After the script is executed, veth0(10.0.0.4) and veth1(10.0.0.5) are created.
veth1 is created in test_namdespace but veth0 in local.

to test ARP response, execute the following steps:
1. `sudo set_veth.sh`
2. edit opt_txsmac in main.c with veth0 MAC address.
3. `sudo ./pv.c veth0 2048 1024 64 64 64 64`
4. `ip netns exec test_namespace arping -I veth1 10.0.0.4`

to remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.


# License
This software is distributed under GPLv2
