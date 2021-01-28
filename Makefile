.PHONY: all libpv.so examples clean

RELEASE ?= 1

all: libpv.so examples

libpv.so:
	make -C libpv RELEASE=$(RELEASE)

examples:
	make -C examples RELEASE=$(RELEASE)

clean:
	make -C examples clean
	make -C libpv clean
