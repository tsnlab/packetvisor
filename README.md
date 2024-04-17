# Packetvisor(PV)

## System dependencies
- clang version >= 11
- llvm version >= 11

## bpftool & XDP-tools (submodules)
PV uses `libxdp` in [XDP-tools][].

[XDP-tools]: https://github.com/xdp-project/xdp-tools

## Build Library
You can use `cargo build -r` command to build PV library and the library file will be located in `target/release/`.

## Getting started
This guide will walk you through the process of compiling and using example source code written with the PV library. \
The following explanation will be based on the Echo example.

Please install the dependent packages first.
```sh
# Install build dependencies
$ sudo apt-get install llvm clang libelf-dev gcc-multilib libpcap-dev m4 make curl

# Install Rust
$ curl https://sh.rustup.rs -sSf | sh
$ source "$HOME/.cargo/env"
```

And compile the example source.
```sh
# Compile all examples.
$ cargo build -r --examples

# Compile the specific example.
$ cargo build -r --example echo
```
The compiled example binaries are located in the `target/release/examples/` directory.

Before running the Echo example, you need to create a Linux network namespace. \
We provide two scripts for this:
- `examples/set_veth.sh` : Create two linux network namespaces.
- `examples/unset_veth.sh` : Delete the two linux network namespaces that were created.

Running the `examples/set_veth.sh` script creates two namespaces(test1 and test2) within the Host.
```sh
# Host veth0 is connected to veth1 in the test1 namespace.
# Host veth2 is connected to veth3 in the test2 namespace.

+-----------+          +-----------+
|   test1   |          |   test2   |
+--[veth1]--+          +--[veth3]--+
      |                      |
+--[veth0]----------------[veth2]--+
|               Host               |
+----------------------------------+
```
Only the test1 namespace is used in the Echo example test.

Now, open two terminals.
- On one terminal (TERM1), run Echo on the veth1 interface of test1 namespace.
- The other terminal (TERM2) sends ARP, ICMP, and UDP packets from Host to test1.

In summary, Host and test1 perform ARP, ICMP, and UDP Echo (Ping-Pong) with each other.

Please execute the following commands on TERM1 and TERM2.
```bash
# Working directory on TERM1 is packetvisor/.
(TERM1) $ sudo ip netns exec test1 /bin/bash
(TERM1) $ ./target/release/examples/echo veth1
-------------------------------------------------
(TERM2) $ sudo apt-get install arping 2ping

# ARP echo test
(TERM2) $ sudo arping 10.0.0.5
# ICMP echo test
(TERM2) $ ping 10.0.0.5
# UDP echo test
(TERM2) $ 2ping 10.0.0.5 --port 7
```
You can now see ARP, ICMP, and UDP (Port 7) packets being echoed in the test1 namespace through the TERM1 logs!

---
## Description of XSK (XDP Socket)

### Elements of XSK
- TX Ring: has descriptors of packets to be sent
- RX Ring: has descriptors of packets to have been received
- Completion Ring (= CQ, Completion queue): has chunks where packets sent successfully are saved
- Filling Ring (= FQ, Filling queue): has chunks which are allocated to receive packets

### RX side procedure
user should pre-allocate chunks which are empty or ready to be used in advance before getting to receive packets and then putting them. the following steps describe how to receive packets through `XSK`.

1. user informs kernel that how many slots should be reserved in `FQ` through `xsk_ring_prod__reserve()`, then the function will return `the number of reserved slots` and `index` value which means the first index of reserved slots in the ring.
2. user puts an `index` in `xsk_ring_prod__fill_addr()` as a parameter and the function will return the pointer of a slot by the `index`. then, user allocates chunk address by putting it to the pointer like `*xsk_ring_prod__fill_addr() = chunk_addr` as much as `the number of reserved slots` with iteration.
3. after the allocation of chunks, `xsk_ring_prod__submit()` will inform kernel that how many chunks are allocated to slots in `FQ`.
4. when some packets are getting received, kernel will copy the packet into chunks as allocation information of `FQ` and it will make descriptors about packets in `RX Ring`.
5. user can know how many packets have been received through `xsk_ring_cons__peek()`. the function will returns `the number of received packets` and `index` value which means the first index of received slots in the ring.
6. with `xsk_ring_cons__rx_desc()`, user can fetch information of received packets by putting the `index` in the function as a parameter.
7. After fetching all information of received packets, user should inform kernel that how many packets in `RX Ring` are consumed through `xsk_ring_cons__release()`. so that, kernel will move `tail` of `RX ring` for the future packet's descriptors to be saved.

### TX side procedure
in contrast to RX side procedure, `CQ` should have enough empty slots before sending packets through `XSK` because `CQ` will be allocated after packets are sent sucessfully. if there is no any slot ready to be allocated in `CQ`, kernel may not able to send packets. user can know what packet has been sent successfully through `CQ`. the following steps describe how to send packets thorugh `XSK`.

1. packets to be sent are ready in chunks.
2. user should reserve slots of `TX Ring` through `xsk_ring_prod__reserve()`. the function will return `the number of reserved slots` and `index` which means the first index of reserved slots.
3. when user puts the `index` in `xsk_ring_prod__tx_desc()` as parameter, the function will return descriptor as the `index`. Then, user puts address of payload and payload length of a packet to be sent into the descriptor.
4. after all descriptors have information about packets, user should inform kernel that how many packets will be sent through `xsk_ring_prod__submit()`.
5. calling `xsk_ring_prod__submit()` alone doesn't send any packet in actual. calling `sendto()` will let kernel to send them.
6. if packets are sent successfully, Kernel will allocate chunks in `CQ` where sent packets used.
7. user can know how many packets are sent through `xsk_ring_cons__peek()` by putting `CQ` as a parameter. the function will return `the number of sent packets` and `index`. the index means the index of the first slot in `CQ`.
8. `xsk_ring_cons__comp_addr()` will return chunk address allocated in `CQ`. and then user can reuse the chunk in the next time.
9. lastly, user should inform kernel how many slots in `CQ` are consumed through `xsk_ring_cons__release()`. so that kernel will move `tail` of `CQ` for the next packet's chunk to be saved in the ring.

# License
This software is distributed under GPLv3 or any later version.

If you need other license than GPLv3 for proprietary use or professional support, please mail us to contact at tsnlab dot com.

# TODO
Make an installer including an option to change capabilities of the application.
use the following command: `sudo setcap CAP_SYS_ADMIN,CAP_NET_ADMIN,CAP_NET_RAW,CAP_DAC_OVERRIDE+ep target/release/pv3_rust`
