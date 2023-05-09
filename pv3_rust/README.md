# Packetvisor 3.0 (Rust)
This is ported in Rust from C

## Prerequisite

### bpftool & XDP-tools (submodules)
PV 3.0(Rust) mainly uses XDP-tools lib and bpftool is used for compiling XDP-tools lib.

The script, `build.sh`, includes submodule update so that you can do the next step.

## Build
You can build PV 3.0 by executing `build.sh`

`./build.sh`

This script includes allowance of capabilities (CAP_SYS_ADMIN, CAP_NET_ADMIN, CAP_NET_RAW, CAP_DAC_OVERRIDE) to `pv3_rust` so that `sudo` is not needed to run the app.

## Use
the app, `pv3_rust`, is located at `/target/release`.
The following is the usage of PV 3.0.

`./pv3_rust <inferface name> <chunk size> <chunk count> <Filling ring size> <RX ring size> <Completion ring size> <TX ring size>`

## Run examples
You can use `set_veth.sh` to set veths for testing the application.
After the script is executed, veth0(10.0.0.4) and veth1(10.0.0.5) are created.
veth1 is created in `test_namespace` namespace but veth0 in local.

to test ARP response, execute the following steps:
1. `sudo set_veth.sh`
2. execute `./pv3_rust veth0 2048 1024 64 64 64 64` in `/target/release`
3. `ip netns exec test_namespace arping -I veth1 10.0.0.4`

to remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.


# License
This software is distributed under GPLv2
