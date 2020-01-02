# CONNX Edge
CONNX Edge is a packet processing framework

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

# License
 * CONNX Edge is licensed under dual license GPLv3 or MIT
 * libbpf is licensed under dual license BSD-2-Clause or LGPL-2.1
 * include/common is copied from xdp-tutorial
