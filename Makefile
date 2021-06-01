.PHONY: all clean bind unbind examples libs

USER_CLFAGS=$(CFLAGS)
USER_CC=$(CC)
USER_DEBUG=$(DEBUG)

CC ?= gcc
AR ?= ar
DEBUG ?= 1

ifneq ($(shell pkg-config --exists libdpdk && echo 0),0)
$(error "DPDK not found")
endif

override CFLAGS += $(shell pkg-config --cflags libdpdk) -L lib/libcollection -llibcl -Ilib/libcollection/include -Iinclude -Wall

ifeq ($(DEBUG), 1)
	override CFLAGS += -O0 -g -DDEBUG=1 -fsanitize=address
else
	override CFLAGS += -O3 -DNDEBUG=1
endif

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, obj/%.o, $(SRCS))
LIBS := lib/libcollection/libcl.a
DEPS := $(patsubst src/%.c, obj/%.d, $(SRCS))
EXAMPLES := $(notdir $(patsubst %/., %, $(wildcard examples/*/.)))

all: libpv.a bin/pv
	for example in $(EXAMPLES); do \
		$(MAKE) -C examples/$$example DEBUG="$(USER_DEBUG)" CFLAGS="$(USER_CFLAGS)" CC="$(USER_CC)"; \
		cp examples/$$example/$$example .; \
	done

bind:
	sudo dpdk-hugepages.py -p $$(( 2 * 1024 * 1024 )) --setup $$(( 256 * 1024 * 1024 ))
	sudo modprobe uio_pci_generic
	sudo dpdk-devbind.py -b uio_pci_generic 00:08.0

unbind:
	sudo dpdk-devbind.py -b e1000 00:08.0
	sudo dpdk-hugepages.py -c
	sudo dpdk-hugepages.py -u

clean:
	for example in $(EXAMPLES); do \
		$(MAKE) -C examples/$$example clean; \
	done
	rm -rf obj
	rm -f $(EXAMPLES)
	rm -f libpv.a

libpv.a: $(OBJS) $(LIBS)
	$(AR) -rcT $@ $^

libs:
	make -C lib/libcollection/ DEBUG=0

src/ver.h:
	bin/ver.sh

obj:
	mkdir -p obj

obj/%.d: src/%.c | obj
	$(CC) $(CFLAGS) -M $< -MT $(@:.d=.o) -MF $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

bin/pv: $(wildcard pypv/*.py)
	python3 -m zipapp -c -o bin/pv pypv -p '/usr/bin/env -S python3 -u'

ifneq (clean,$(filter clean, $(MAKECMDGOALS)))
-include $(DEPS)
endif
