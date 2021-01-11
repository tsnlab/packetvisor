.PHONY: shared clean

APP = init

SRCS := main.c

all: shared

shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)

PKGCONF ?= pkg-config

CFLAGS += -Wl,-rpath=/home/user/packetvisor/libpv
LDFLAGS_SHARED = -L../../libpv -lpv $(shell $(PKGCONF) --libs libdpdk)
INCLUDE = -I ../../lipv/include


build/$(APP)-shared: $(SRCS) | build
	$(CC) $(CFLAGS) $(SRCS) -o $@ -I ../../libpv/include $(LDFLAGS) $(LDFLAGS_SHARED)

build:
	@mkdir -p $@

clean:
	rm -rf build

