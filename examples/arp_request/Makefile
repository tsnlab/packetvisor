.PHONY: shared clean

APP = arp_request
RELEASE ?= 1

ifeq ($(PV_HOME),)
PV_HOME = $(word 1, $(subst /examples, , $(realpath $(lastword $(MAKEFILE_LIST)))))
endif

ifeq ($(RELEASE), 1)
	CFLAGS=-Wl,-rpath=$(PV_HOME)/libpv -O3
else
	CFLAGS=-Wl,-rpath=$(PV_HOME)/libpv -O0 -g -fsanitize=address
endif

SRCS := main.c

all: shared

shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
	cp build/$(APP) $(APP)

PKGCONF ?= pkg-config

LDFLAGS_SHARED = -L $(PV_HOME)/libpv -lpv
INCLUDE = -I $(PV_HOME)/libpv/include


build/$(APP)-shared: $(SRCS) | build
	$(CC) $(CFLAGS) $(SRCS) -o $@ $(INCLUDE) $(LDFLAGS) $(LDFLAGS_SHARED)

build:
	@mkdir -p $@

clean:
	rm -rf build
	rm -f $(APP)

