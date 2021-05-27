# Packetvisor 2.1

## Prerequisite

### DPDK
PV 2.1 is tested on DPDK 20.11.1 LTS. Below is very short introduction to compile DPDK on Ubuntu 20.04.

 1. sudo apt install -qy meson ninja-build python3-pyelftools
 2. On some AMD machines some kernel parameter is needed \
    iommu=pt amd_iommu=on
 3. meson build # In DPDK source directory
 4. ninja -C build
 5. sudo ninja -C build install

## Build
Just type make. It will build libpv.a and examples also.

$ make # Build libpv.a and examples \
$ make clean # Clean the generated files

## Allocate some resources
You need to allocate some reosurces before run applications.

$ make bind

 1. It will allocate huge pages
 2. It will load uio kernel modules
 3. It will bind kernel module to specific NIC

## Deallocate resources
$ make unbind

## Run examples
$ make bind
$ sudo ./dump # Running dump example
$ make unbind

## Examples
### dump
Dump example dumps all the received packets. It prints ethernet header and size of the packet received.

$ sudo ./dump

### echo
Echo example responses ARP, ICMPv4 and UDP.

$ sudo ./echo [ip address]

To test the example run below command from other computer. \
$ arping [ip address] # ARP echo test \
$ ping [ip address] # ICMP echo test \
$ python bin/udp\_echo\_cli.py [ip address] 7 # UDP echo test, 7 is the port number \
  send: [Input any message to send] \
  recv: [Your message will be here]

# License
This software is distributed under GPLv3
