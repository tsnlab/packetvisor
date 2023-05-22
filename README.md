# Packetvisor 3.0

## Prerequisite

### System dependencies

- clang (clang-strip)
- llvm
- libelf-dev (gelf.h)
- libpcap-dev
- gcc-multilib (asm/types.h)

### bpftool & XDP-tools (submodules)
PV 3.0 uses `libxdp` in [XDP-tools][].

[XDP-tools]: https://github.com/xdp-project/xdp-tools

## Build
You can use `cargo build -r` command to build PV 3.0.

## Use
the app, `pv3_rust`, is located at `/target/release`.
The following is the usage of PV 3.0.

`./pv3_rust <inferface name> <chunk size> <chunk count> <Filling ring size> <RX ring size> <Completion ring size> <TX ring size>`

## Run examples
You can use `set_veth.sh` to set veths for testing the application.
After the script is executed, veth0(10.0.0.4) and veth1(10.0.0.5) are created.
veth1 is created in `test_namespace` namespace but veth0 in local.

to test ARP response, execute the following steps:
1. `sudo set_veth.sh`
2. execute `./pv3_rust veth0 2048 1024 64 64 64 64` in `/target/release`
3. `ip netns exec test_namespace arping -I veth1 10.0.0.4`

to remove veths created by `set_veth.sh`, `unset_veth.sh` will remove them.

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
This software is distributed under GPLv2

# TODO
Make an installer including an option to change capabilities of the application.    
use the following command: `sudo setcap CAP_SYS_ADMIN,CAP_NET_ADMIN,CAP_NET_RAW,CAP_DAC_OVERRIDE+ep target/release/pv3_rust`
