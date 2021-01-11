.PHONY: all libpv/libpv.so examples

RELEASE ?= 0

all: libpv.so examples

libpv/libpv.so:
	make -C libpv RELEASE=$(RELEASE)

libpv.so:
	cp $^ $@

examples:
	make -C examples
