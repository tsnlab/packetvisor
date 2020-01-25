# Packetvisor
Packetvisor is a packet processing framework

# Install
## libbpf
git submodule init
git submodule update

## clang, llvm, etc
sudo apt install clang llvm gcc-multilib  libelf-dev libpugixml-dev

On Debian 10(stable release)
sudo apt install ethtool

On Debian 11(testing release)
sudo apt install linux-tools-$(uname -r)

## Linux header
linux-headers-$(uname -r)

# Compile
make		# for debug
make RELEASE=1	# for release

# Run
There must be need 2 terminals. Terminal #1 will be client, Terminal #2 will be server.
Terminal #1 will have veth0 interface which is connected to Terminal #2's veth interface.
Terminal #2 will have veth interface which is connected to terminal #1's veth0 interface.
pv.o module will installed in veth interface.

## Terminal #2 (ping server)
make setup		-> This create veth interface
make run		-> Install xdp module to kernel, and launch user space application

make off		-> Uninstall xdp moudle forcely 
				   when "ERROR: Can't setup AF_XDP socket "Device or resource busy" occurs
make teardown	-> destroy veth interface

## Terminal #1 (ping client)
make etner								-> enter virtual environment
ip addr add 192.168.0.10/24 dev veth0	-> set veth0's IPv4 address
ping 192.168.0.1						-> ping to XDP's user application

## Debugging
sudo wireshark -k -i /tmp/pv			-> Capture packets

# License
 * packetngin (xdp_user) is licensed under dual license GPLv3 or MIT
 * pv.so (pv) is licensed under dual license LGPLv3 or MIT
 * pv.o (xdp_kern) is licensed under GPLv2
 * libbpf is licensed under dual license BSD-2-Clause or LGPL-2.1
 * xdp_user/include/common is copied from xdp-tutorial

# TODO list
- [X] ARP ping use case
- [X] ICMP ping use case
- [X] xdp_user need to be compiled with g++ not gcc -> This is why exception is not working now
- [X] C code to OOP
- [X] Dynamic packetlet loading
- [ ] all of the xdp-tutorial code must be removed
- [ ] multiple queue, interfaces
- [ ] acceleration
- [ ] load balancer
