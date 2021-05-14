.PHONY: all run clean

TARGET=libpv
DEBUG ?= 1
CC=gcc

CFLAGS=-Iinclude -Wall -fPIC -O3

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g -fsanitize=address
endif

PKGCONF ?= pkg-config

LIBS=-ldl
CFLAGS += $(shell $(PKGCONF) --cflags libdpdk libfyaml)
LIBS += $(shell $(PKGCONF) --libs libdpdk libfyaml)

OBJS=$(patsubst src/%.c, obj/%.o, $(wildcard src/*.c))
DEPS=$(OBJS:.o=.d)

all: $(TARGET).so

clean:
	rm -rf obj
	rm -f $(TARGET).so

$(TARGET).so: $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(filter %.o, $^) $(LIBS)

$(TARGET).a: $(OBJS)
	ar rc $@ $(filter %.o, $^)

obj:
	mkdir -p obj

obj/%.d : src/%.c | obj
	$(CC) $(CFLAGS) -M -MT $(@:.d=.o) $< -o $@

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c -o $@ $<

ifneq (clean,$(filter clean,$(MAKECMDGOALS)))
-include $(DEPS)
endif
