#!/bin/bash
#sudo ./build/init -l 1 -d /usr/local/lib/x86_64-linux-gnu/librte_net_e1000.so
#sudo ./build/init -l 1 -d /usr/local/lib/x86_64-linux-gnu/librte_net_e1000.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool.so
sudo ./build/init -l 1 -d /usr/local/lib/x86_64-linux-gnu/librte_net_e1000.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_bucket.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_dpaa.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_dpaa2.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_octeontx2.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_octeontx.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_ring.so -d /usr/local/lib/x86_64-linux-gnu/librte_mempool_stack.so
