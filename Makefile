.PHONY: all run test clean cleanall

RELEASE ?= 0
CC=gcc
ifeq ($(RELEASE), 1)
	CFLAGS=-Iinclude -Ilibbpf/include -Ilibbpf/include/uapi -Llibbpf/src -Wall -std=gnu99 -O3
else
	CFLAGS=-Iinclude -Ilibbpf/include -Ilibbpf/include/uapi -Llibbpf/src -Wall -std=gnu99 -O0 -g -fsanitize=address
endif

LIBS=-Wl,-Bstatic -lbpf -lz -Wl,-Bdynamic -lelf -lpthread -Wl,--as-needed

OBJS=$(patsubst src/%.c, obj/%.o, $(filter-out src/xdp_edge.c, $(wildcard src/*.c)))

all: edge xdp_edge.o

run: all
	./edge

clean:
#	make -C test clean
	rm -rf obj
	rm -f edge
	rm -f xdp_edge.o

cleanall: clean

#test: all
#	make -C test run

edge: libbpf/src/libbpf.a $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(filter %.o, $^) $(LIBS)


#		-I/usr/src/$(shell uname -r)/include/uapi \
#		-Ilibbpf/include \

obj/xdp_edge.ll: src/xdp_edge.c
	mkdir -p obj
	clang -S \
		-target bpf \
		-D __BPF_TRACING__ \
		-Ilibbpf/include/uapi \
		-Iinclude \
		-Wall \
		-Wno-unused-value \
		-Wno-pointer-sign \
		-Wno-compare-distinct-pointer-types \
		-Werror \
		-O2 -emit-llvm -c -g -o $@ $^

xdp_edge.o: obj/xdp_edge.ll
	llc -march=bpf -filetype=obj -o $@ $^

libbpf/src/libbpf.a:
	cd libbpf/src; make

obj/%.d : src/%.c src/onnx.proto3.pb-c.c
	mkdir -p obj; $(CC) $(CFLAGS) -M $< > $@

-include $(patsubst src/%.c, obj/%.d, $(wildcard src/*.c))  

obj/%.o: src/%.c
	mkdir -p obj; $(CC) $(CFLAGS) -c -o $@ $^
