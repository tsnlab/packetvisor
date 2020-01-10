# Packetvisor
Packetvisor is a packet processing framework

# Install
## libbpf
git submodule init
git submodule update

## clang, llvm, etc
sudo apt install clang llvm libelf-dev gcc-multilib linux-tools-$(uname -r)

## Linux kernel header
sudo apt install linux-headers-$(uname -r)

# Compile
make			# for debug
make RELEASE=1	# for release

# Run
make run

When "ERROR: Can't setup AF_XDP socket "Device or resource busy" occurs

make off

# License
 * packetngin (xdp_user) is licensed under dual license GPLv3 or MIT
 * pv.so (pv) is licensed under dual license LGPLv3 or MIT
 * pv.o (xdp_kern) is licensed under GPLv2
 * libbpf is licensed under dual license BSD-2-Clause or LGPL-2.1
 * xdp_user/include/common is copied from xdp-tutorial

# TODO list
- [ ] ARP ping use case
- [ ] ICMP ping use case
- [ ] xdp_user need to compiled with g++ not gcc -> This is why exception is not working now
- [ ] all of the xdp-tutorial code must be removed
- [ ] multiple queue, interfaces
- [ ] acceleration
- [ ] load balancer
