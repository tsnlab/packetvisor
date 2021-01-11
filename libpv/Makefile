.PHONY: all run clean

TARGET=libpv
RELEASE ?= 1
CC=gcc

ifeq ($(RELEASE), 1)
	CFLAGS=-Iinclude -Wall -fPIC -O3
else
	CFLAGS=-Iinclude -Wall -fPIC -O0 -g -fsanitize=address
endif

PKGCONF ?= pkg-config

LIBS=-ldl
CFLAGS += $(shell $(PKGCONF) --cflags libdpdk)
LIBS += $(shell $(PKGCONF) --libs libdpdk) -lrte_net_e1000

OBJS=$(patsubst src/%.c, obj/%.o, $(wildcard src/*.c))

all: $(TARGET).so

clean:
	rm -rf obj
	rm -f $(TARGET).so

$(TARGET).so: $(OBJS)
	$(CC) $(CFLAGS) -shared -o $@ $(filter %.o, $^)

$(TARGET).a: $(OBJS)
	ar rc $@ $(filter %.o, $^)

obj/%.d : src/%.c
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.c, obj/%.d, $(wildcard src/*.c))  

obj/%.o: src/%.c
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^
