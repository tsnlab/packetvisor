.PHONY: all run clean pv/libpv.so xdp_user/packetvisor xdp_kern/pv.o

RELEASE ?= 0

all: libbpf.so libpv.so pv.o packetvisor

run: all
	sudo LD_LIBRARY_PATH=. ./packetvisor -d lo --filename pv.o

clean:
	rm -f libbpf.so*
	rm -f libpv.so pv.o packetvisor
	make -C pv clean
	make -C xdp_user clean
	make -C xdp_kern clean

libbpf/src/libbpf.so:
	make -C libbpf/src

libbpf.so: libbpf/src/libbpf.so
	cp libbpf/src/libbpf.so* .

pv/libpv.so:
	make -C pv RELEASE=$(RELEASE)

libpv.so: pv/libpv.so
	cp $^ $@

xdp_user/packetvisor:
	make -C xdp_user RELEASE=$(RELEASE)

packetvisor: xdp_user/packetvisor
	cp $^ $@

xdp_kern/pv.o:
	make -C xdp_kern RELEASE=$(RELEASE)

pv.o: xdp_kern/pv.o
	cp $^ $@
