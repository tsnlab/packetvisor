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

### Run UDP echo

```sh
# assume packetvisor host's IP is 192.168.1.11

# On packetvisor host
$ make && sudo python3 pv.py examples/udp_echo/udp_echo

# On client host
# ping to check ICMP echo
$ ping 192.168.1.11
# UDP echo test
$ ./examples/udp_echo/echo_cli 192.168.1.11 8080
send: abcd
recv: abcd

# If you want to set source port
$ ./examples/udp_echo/echo_cli 192.168.1.11 8080 --bind 9999
```
