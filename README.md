# Packetvisor 2.0

## Prerequisite

### DPDK
- Install DPDK
- Tested on DPDK 21.02.0-rc0

### libfyaml
- Refer to 'https://github.com/pantoniou/libfyaml'
- After installation, might need to run 'ldconfig' at libfyaml project root directory.

## Build
Run 'make' at project root directory.

## Run app
Use 'pv.py' script to run app. Need root privilage.
ex) $ sudo ./pv.py examples/arp_request/arp_request
