#!/bin/sh

echo "update submodule"
git submodule update --init --recursive

echo "compile bpftool and copy it"
make -C ../bpftool/src
cp ../bpftool/src/bpftool /usr/sbin

echo "compile xdp-tools libraries and utilities"
make -C ../xdp-tools

echo "compile userspace application"
clang -Wall -O2 -g \
    -I../xdp-tools/lib/libbpf/src \
    -I../xdp-tools/lib/libxdp/ \
    -I../xdp-tools/lib/util \
    -I../xdp-tools/headers/ \
    -I../xdp-tools/lib/libbpf/src/root/usr/include \
    -L../xdp-tools/lib/libbpf/src \
    -L../xdp-tools/lib/libxdp/ \
    -o pv \
    main.c arp.c pv.c \
    -l:libxdp.a -l:libbpf.a -lelf -lz
echo "done"
