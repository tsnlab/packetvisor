# SPDX-License-Identifier: (GPL-2.0 OR BSD-2-Clause)

# Default values - can be overridden by build.rs
CLANG ?= clang
LLC ?= llc
BPF_TARGET ?= bpf

# Source and output files
XDP_SRC := packervisor.bpf.c
XDP_OBJ := packervisor.bpf.o
XDP_LL := packervisor.bpf.ll

# Header directories (will be set by build.rs)
HEADER_DIR ?= ../xdp-tools/headers
INCLUDE_DIR ?= ../xdp-tools/lib/libbpf/src
BPF_HEADERS := $(wildcard $(HEADER_DIR)/bpf/*.h) $(wildcard $(HEADER_DIR)/xdp/*.h)

# Compiler flags
BPF_CFLAGS := -I$(HEADER_DIR) \
	      -I$(INCLUDE_DIR) \
	      -D __BPF_TRACING__ \
	      -Wno-unused-value \
	      -Wno-pointer-sign \
	      -Wno-compare-distinct-pointer-types \
	      -O2 \
	      -g

.PHONY: all clean

all: $(XDP_OBJ)

$(XDP_OBJ): $(XDP_SRC) $(BPF_HEADERS)
	$(CLANG) -S \
	    -target $(BPF_TARGET) \
	    $(BPF_CFLAGS) \
	    -Wall \
	    -Werror \
	    -emit-llvm -c -g -o $(XDP_LL) $<
	$(LLC) -march=$(BPF_TARGET) -filetype=obj -o $@ $(XDP_LL)

clean:
	rm -f $(XDP_OBJ) $(XDP_LL)

