#!/bin/bash

sudo ./build/l2_fwd -l 1 -d /usr/local/lib/x86_64-linux-gnu/librte_net_ixgbe.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_ring.so
