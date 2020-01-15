.PHONY: all run test clean

TARGET=pv
RELEASE ?= 0
CC=g++
ifeq ($(RELEASE), 1)
	#CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -Wall -std=gnu99 -O3
	CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -Wall -std=c++11 -O3
else
	#CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -Wall -std=gnu99 -O0 -g -fsanitize=address
	CFLAGS=-Iinclude -I../libbpf/include -I../libbpf/include/uapi -L../libbpf/src -Wall -std=c++11 -O0 -g -fsanitize=address
endif

LIBS=-Wl,-Bstatic -Wl,-Bdynamic -lbpf -lz -lelf -lpthread -ldl -Wl,--as-needed

OBJS=$(patsubst src/%.c, obj/%.o,$(wildcard src/*.c))

all: $(TARGET)

run: all
	sudo ./$(TARGET) -d lo --filename xdp_kern.o

clean:
	rm -rf obj
	rm -f $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(filter %.o, $^) $(LIBS)

obj/%.d : src/%.c
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.c, obj/%.d, $(wildcard src/*.c))  

obj/%.o: src/%.c
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^
