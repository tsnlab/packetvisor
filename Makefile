.PHONY: all libpv.so examples clean

DEBUG ?= 1

all: libpv.so examples

libpv.so:
	make -C libpv DEBUG=$(RELEASE)

examples:
	make -C examples DEBUG=$(RELEASE)

clean:
	make -C examples clean
	make -C libpv clean
