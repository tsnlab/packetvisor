.PHONY: all clean bind unbind

CC ?= gcc
AR ?= ar
DEBUG ?= 1

ifneq ($(shell pkg-config --exists libdpdk && echo 0),0)
$(error "DPDK not found")
endif

override CFLAGS += $(shell pkg-config --cflags libdpdk) -Iinclude -Wall

ifeq ($(DEBUG), 1)
	override CFLAGS += -pg -O0 -g -DDEBUG=1 -fsanitize=address
else
	override CFLAGS += -O3
endif

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, obj/%.o, $(SRCS))
DEPS := $(patsubst src/%.c, obj/%.d, $(SRCS))
EXAMPLES := $(notdir $(wildcard examples/*))

all: libpv.a $(EXAMPLES)

bind:
	sudo dpdk-hugepages.py -p $$(( 2 * 1024 * 1024 )) --setup $$(( 256 * 1024 * 1024 ))
	sudo modprobe uio_pci_generic
	sudo dpdk-devbind.py -b uio_pci_generic 00:08.0

unbind:
	sudo dpdk-devbind.py -u 00:08.0
	sudo dpdk-hugepages.py -c
	sudo dpdk-hugepages.py -u

$(EXAMPLES):
	$(MAKE) -C examples/$@
	cp examples/$@/$@ .

clean:
	for example in $(EXAMPLES); do \
		$(MAKE) -C examples/$$example clean; \
	done
	rm -rf obj
	rm -f $(EXAMPLES)
	rm -f libpv.a

libpv.a: $(OBJS)
	$(AR) rcs $@ $^

src/ver.h:
	bin/ver.sh

obj/%.d: $(SRCS) src/ver.h
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $^

ifneq (clean,$(filter clean, $(MAKECMDGOALS)))
-include $(DEPS)
endif
