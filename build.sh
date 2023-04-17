#!/bin/sh

echo "update submodule"
git submodule update --init --recursive

echo "compile bpftool and copy it"
make -C bpftool/src
cp bpftool/src/bpftool /usr/sbin

echo "compile xdp-tools libraries and utilities"
make -C xdp-tools

echo "compile userspace application"
clang -Wall -O2 -g \
    -Ixdp-tools/lib/libbpf/src \
    -Ixdp-tools/lib/libxdp/ \
    -Ixdp-tools/lib/util \
    -Ixdp-tools/headers/ \
    -Ixdp-tools/lib/libbpf/src/root/usr/include \
    -Lxdp-tools/lib/libbpf/src \
    -Lxdp-tools/lib/libxdp/ \
    -o src/pv \
    src/main.c src/arp.c src/pv.c \
    -l:libxdp.a -l:libbpf.a -lelf -lz
echo "done"
