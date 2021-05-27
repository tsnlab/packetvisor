.PHONY: all clean

CC ?= gcc
DEBUG ?= 1
PV_INCLUDE_PATH ?= ../../include
PV_LIB_PATH ?= ../..

ifneq ($(shell pkg-config --exists libdpdk && echo 0),0)
$(error "DPDK not found")
endif

override CFLAGS += $(shell pkg-config --cflags libdpdk) -Iinclude -I$(PV_INCLUDE_PATH) -Wall

ifeq ($(DEBUG), 1)
	override CFLAGS += -pg -O0 -g -DDEBUG=1 -fsanitize=address
else
	override CFLAGS += -O3
endif

LIBS := -L $(PV_LIB_PATH) -lpv $(shell pkg-config --libs libdpdk)
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c, obj/%.o, $(SRCS))
DEPS := $(patsubst src/%.c, obj/%.d, $(SRCS))
APP := $(notdir $(CURDIR))

all: $(APP)

$(APP): $(OBJS) $(PV_LIB_PATH)/libpv.a
	$(CC) $(CFLAGS) -o $@ $(filter %.o, $^) $(LIBS)

clean:
	rm -rf obj
	rm -f $(APP)

obj:
	mkdir -p obj

obj/%.d: src/%.c | obj
	$(CC) $(CFLAGS) -M $< -MT $(@:.d=.o) -MF $@

obj/%.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

ifneq (clean,$(filter clean, $(MAKECMDGOALS)))
-include $(DEPS)
endif
