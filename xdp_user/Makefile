.PHONY: all run test clean

TARGET=pv
RELEASE ?= 0
CC=g++
ifeq ($(RELEASE), 1)
	CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -L../libpv -Wall -std=c++11 -O3
else
	CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -L../libpv -Wall -std=c++11 -O0 -g -fsanitize=address
endif

LIBS=-lbpf -lz -lelf -lpthread -ldl -lpv #-Wl,-Bstatic -Wl,-Bdynamic -lbpf -lz -lelf -lpthread -ldl -Wl,--as-needed

OBJS=$(patsubst src/%.cpp, obj/%.o,$(wildcard src/*.cpp))

all: $(TARGET)

run: all
	sudo ./$(TARGET) -d lo --filename xdp_kern.o

clean:
	rm -rf obj
	rm -f $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(filter %.o, $^) $(LIBS)

obj/%.d : src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.cpp, obj/%.d, $(wildcard src/*.cpp))  

obj/%.o: src/%.cpp
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^
