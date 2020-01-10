.PHONY: all setup teardown enter run off clean libpv/libpv.so xdp_user/pv xdp_kern/pv.o

RELEASE ?= 0
ENV ?= veth
MAC ?= ${shell ip addr | sed -n '/$(ENV)/{N; p}' | grep ether | awk '{print $$2}'}

all: libbpf.so libpv.so pv.o pv

setup:
	sudo testenv/testenv.sh setup --name $(ENV)

teardown:
	sudo testenv/testenv.sh teardown

enter:
	sudo testenv/testenv.sh enter --name $(ENV)

run: all
	sudo LD_LIBRARY_PATH=. ./pv -d $(ENV) -m $(MAC) --filename pv.o

off:
	sudo ip link set dev $(ENV) xdpgeneric off

clean:
	rm -f libbpf.so*
	rm -f libpv.so pv.o pv
	make -C libpv clean
	make -C xdp_user clean
	make -C xdp_kern clean

libbpf/src/libbpf.so:
	make -C libbpf/src

libbpf.so: libbpf/src/libbpf.so
	cp libbpf/src/libbpf.so* .

libpv/libpv.so:
	make -C libpv RELEASE=$(RELEASE)

libpv.so: libpv/libpv.so
	cp $^ $@

xdp_user/pv:
	make -C xdp_user RELEASE=$(RELEASE)

pv: xdp_user/pv
	cp $^ $@

xdp_kern/pv.o:
	make -C xdp_kern RELEASE=$(RELEASE)

pv.o: xdp_kern/pv.o
	cp $^ $@
