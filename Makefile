.PHONY: all libpv.so examples clean

DEBUG ?= 1

all: libpv.so examples

libpv.so:
	make -C libpv DEBUG=$(DEBUG)

examples:
	make -C examples DEBUG=$(DEBUG)

clean:
	make -C examples clean
	make -C libpv clean
