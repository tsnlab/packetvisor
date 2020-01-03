.PHONY: all clean

TARGET=pv.o

all: $(TARGET)

clean:
	rm -rf obj
	rm -f $(TARGET)

obj/pv.ll: src/pv.c
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

$(TARGET): obj/pv.ll
	llc -march=bpf -filetype=obj -o $@ $^
