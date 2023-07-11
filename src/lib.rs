mod bindings {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    #![allow(clippy::all)]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

use bindings::*;
use core::ffi::*;
use pnet::datalink::{interfaces, NetworkInterface};
use std::alloc::{alloc_zeroed, Layout};
use std::ffi::CString;
use std::ptr::copy;

const DEFAULT_HEADROOM: usize = 256;

#[derive(Debug, Clone, Copy)]
pub struct Packet {
    pub start: usize,       // payload offset pointing the start of payload. ex. 256
    pub end: usize,         // payload offset point the end of payload. ex. 1000
    pub buffer_size: usize, // total size of buffer. ex. 2048
    pub buffer: *mut u8,    // buffer address.
    private: *mut c_void,   // DO NOT touch this.
}

impl Packet {
    fn new() -> Packet {
        Packet {
            start: 0,
            end: 0,
            buffer_size: 0,
            buffer: std::ptr::null_mut(),
            private: std::ptr::null_mut(),
        }
    }

    // replace payload with new data
    pub fn replace_data(&mut self, new_data: &Vec<u8>) -> Result<(), String> {
        if new_data.len() <= self.buffer_size {
            unsafe {
                // replace data
                copy(new_data.as_ptr(), self.buffer.offset(0), new_data.len());
                self.start = 0;
                self.end = new_data.len();

                Ok(())
            }
        } else {
            Err(String::from(
                "Data size is over than buffer size of packet.",
            ))
        }
    }

    pub fn get_buffer_mut(&mut self) -> &mut [u8] {
        unsafe {
            std::slice::from_raw_parts_mut(
                self.buffer.offset(self.start.try_into().unwrap()),
                self.end - self.start,
            )
        }
    }

    pub fn resize(&mut self, size: usize) {
        self.end = self.start + size;
    }

    #[allow(dead_code)]
    pub fn dump(&self) {
        let chunk_address = self.private as u64;
        let buffer_address: *const u8 = self.buffer.cast_const();

        let length: usize = self.buffer_size;
        let mut count: usize = 0;

        unsafe {
            println!("---packet dump--- chunk addr: {}", chunk_address);

            loop {
                let read_offset: usize = count;
                let read_address: *const u8 = buffer_address.add(read_offset);
                print!("{:02X?} ", std::ptr::read(read_address));

                count += 1;
                if count == length {
                    break;
                } else if count % 8 == 0 {
                    print!(" ");
                    if count % 16 == 0 {
                        println!();
                    }
                }
            }
        }
        println!("\n-------\n");
    }
}

#[derive(Debug)]
pub struct NIC {
    pub interface: NetworkInterface,
    xsk: *mut xsk_socket,

    umem: *mut xsk_umem,
    buffer: *mut c_void,
    chunk_size: usize,

    chunk_pool: Vec<u64>,
    chunk_pool_idx: usize,

    /* XSK rings */
    fq: xsk_ring_prod,
    rx: xsk_ring_cons,
    cq: xsk_ring_cons,
    tx: xsk_ring_prod,
}

impl NIC {
    pub fn new(
        if_name: &str,
        set_chunk_size: usize,
        set_chunk_count: usize,
    ) -> Result<NIC, String> {
        let mut is_creation_failed: bool = false;

        let xsk_ptr;
        unsafe {
            let xsk_layout = Layout::new::<xsk_socket>();
            xsk_ptr = alloc_zeroed(xsk_layout);

            if xsk_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        let umem_ptr;
        unsafe {
            let umem_layout = Layout::new::<xsk_umem>();
            umem_ptr = alloc_zeroed(umem_layout);

            if umem_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        let fq_ptr;
        unsafe {
            let fq_layout = Layout::new::<xsk_ring_prod>();
            fq_ptr = alloc_zeroed(fq_layout);

            if fq_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        let rx_ptr;
        unsafe {
            let rx_layout = Layout::new::<xsk_ring_cons>();
            rx_ptr = alloc_zeroed(rx_layout);

            if rx_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        let cq_ptr;
        unsafe {
            let cq_layout = Layout::new::<xsk_ring_cons>();
            cq_ptr = alloc_zeroed(cq_layout);

            if cq_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        let tx_ptr;
        unsafe {
            let tx_layout = Layout::new::<xsk_ring_prod>();
            tx_ptr = alloc_zeroed(tx_layout);

            if tx_ptr.is_null() {
                is_creation_failed = true;
            }
        }

        if is_creation_failed {
            Err(String::from("Failed to allocate memory for NIC."))
        } else {
            /* check interface name to be attached XDP program is valid */
            let all_interfaces: Vec<NetworkInterface> = interfaces();
            let matched_interface: Option<&NetworkInterface> = all_interfaces
                .iter()
                .find(|element| element.name.as_str() == if_name);
            match matched_interface {
                Some(a) => {
                    let nic: NIC = unsafe {
                        NIC {
                            interface: a.clone(),
                            xsk: xsk_ptr.cast::<xsk_socket>(),
                            umem: umem_ptr.cast::<xsk_umem>(), // umem is needed to be dealloc after using packetvisor library.
                            buffer: std::ptr::null_mut(),
                            chunk_size: set_chunk_size,
                            chunk_pool: Vec::with_capacity(set_chunk_count),
                            chunk_pool_idx: set_chunk_count,
                            fq: std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()),
                            rx: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                            cq: std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()),
                            tx: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
                        }
                    };

                    Ok(nic)
                }
                None => Err(String::from("Invalid interface name.")),
            }
        }
    }

    pub fn copy(&mut self, target: &Packet) -> Option<Packet> {
        let add = self.alloc();
        let len = target.end - target.start;
        match add {
            Some(mut packet) => {
                unsafe {
                    std::ptr::copy_nonoverlapping(target.buffer.add(target.start), packet.buffer.add(packet.start), len);
                }
                packet.end = packet.start + len;
                self.chunk_pool.push(target.private as u64);
                Some(packet)
            }
            None => {
                println!("Failed to add packet to NIC.");
                None
            }
        }
    }

    fn alloc_idx(&mut self) -> u64 {
        if self.chunk_pool_idx > 0 {
            self.chunk_pool_idx -= 1;
            self.chunk_pool[self.chunk_pool_idx]
        } else {
            u64::MAX
        }
    }

    pub fn alloc(&mut self) -> Option<Packet> {
        let idx: u64 = self.alloc_idx();

        match idx {
            u64::MAX => None,
            _ => {
                let mut packet: Packet = Packet::new();
                packet.start = DEFAULT_HEADROOM;
                packet.end = DEFAULT_HEADROOM;
                packet.buffer_size = self.chunk_size;
                packet.buffer = unsafe { xsk_umem__get_data(self.buffer, idx).cast::<u8>() };
                packet.private = idx as *mut c_void;

                Some(packet)
            }
        }
    }

    fn free_idx(&mut self, chunk_addr: u64) {
        // align **chunk_addr with chunk size
        let remainder = chunk_addr % (self.chunk_size as u64);
        let chunk_addr = chunk_addr - remainder;
        self.chunk_pool[self.chunk_pool_idx] = chunk_addr;
        self.chunk_pool_idx += 1;
    }

    pub fn free(&mut self, packet: &Packet) {
        self.free_idx(packet.private as u64);
    }

    fn configure_umem(
        &mut self,
        set_chunk_size: usize,
        set_chunk_count: usize,
        set_fill_size: u32,
        set_complete_size: u32,
    ) -> Result<(), i32> {
        let umem_buffer_size: usize = set_chunk_count * set_chunk_size; // chunk_count is UMEM size.

        /* Reserve memory for the UMEM. */
        let mmap_address = unsafe {
            libc::mmap(
                std::ptr::null_mut::<libc::c_void>(),
                umem_buffer_size,
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_PRIVATE | libc::MAP_ANONYMOUS,
                -1,
                0,
            )
        };

        if mmap_address == libc::MAP_FAILED {
            return Err(-1);
        }

        /* create UMEM, filling ring, completion ring for xsk */
        let umem_cfg: xsk_umem_config = xsk_umem_config {
            // ring sizes aren't usually over 1024
            fill_size: set_fill_size,
            comp_size: set_complete_size,
            frame_size: set_chunk_size as u32,
            frame_headroom: XSK_UMEM__DEFAULT_FRAME_HEADROOM,
            flags: XSK_UMEM__DEFAULT_FLAGS,
        };

        let ret: c_int = unsafe {
            xsk_umem__create(
                &mut self.umem,
                mmap_address,
                umem_buffer_size as u64,
                &mut self.fq,
                &mut self.cq,
                &umem_cfg,
            )
        };
        match ret {
            0 => {
                self.buffer = mmap_address;
                Ok(())
            }
            _ => {
                unsafe {
                    libc::munmap(mmap_address, umem_buffer_size);
                }
                Err(ret)
            }
        }
    }

    pub fn open(
        &mut self,
        rx_ring_size: u32,
        tx_ring_size: u32,
        filling_ring_size: u32,
        completion_ring_size: u32,
    ) -> Result<(), String> {
        /* initialize UMEM chunk information */
        for i in 0..self.chunk_pool.capacity() {
            self.chunk_pool.push((i * self.chunk_size) as u64); // put chunk address
        }

        /* configure UMEM */
        let ret = self.configure_umem(
            self.chunk_size,
            self.chunk_pool.capacity(),
            filling_ring_size,
            completion_ring_size,
        );
        if ret.is_err() {
            return Err(format!("Failed to configure UMEM: {}", ret.unwrap_err()));
        }

        /* pre-allocate UMEM chunks into fq */
        let mut fq_idx: u32 = 0;
        let reserved: u32 =
            unsafe { xsk_ring_prod__reserve(&mut self.fq, filling_ring_size, &mut fq_idx) };

        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(&mut self.fq, fq_idx + i) = self.alloc_idx();
            } // allocation of UMEM chunks into fq.
        }
        unsafe {
            xsk_ring_prod__submit(&mut self.fq, reserved);
        } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

        /* setting xsk, RX ring, TX ring configuration */
        let xsk_cfg: xsk_socket_config = xsk_socket_config {
            rx_size: rx_ring_size,
            tx_size: tx_ring_size,
            /* zero means loading default XDP program.
            if you need to load other XDP program, set 1 on this flag and use xdp_program__open_file(), xdp_program__attach() in libxdp. */
            __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 { libxdp_flags: 0 },
            xdp_flags: 4, // XDP_FLAGS_DRV_MODE in if_link.h (driver mode (Native mode))
            bind_flags: XDP_USE_NEED_WAKEUP as u16,
        };
        let if_name = CString::new(self.interface.name.clone()).unwrap();
        let if_ptr = if_name.as_ptr() as *const c_char;
        /* create xsk socket */
        let ret: c_int = unsafe {
            xsk_socket__create(
                &mut self.xsk,
                if_ptr,
                // self.interface.name.as_ptr().clone() as *const c_char,
                0,
                self.umem,
                &mut self.rx,
                &mut self.tx,
                &xsk_cfg,
            )
        };
        if ret != 0 {
            return Err(format!(
                "xsk_socket__create failed: {}",
                std::io::Error::last_os_error()
            ));
        }
        Ok(())
    }

    // move ownership of nic
    pub fn close(self) -> c_int {
        /* xsk delete */
        unsafe {
            xsk_socket__delete(self.xsk);
        }

        /* UMEM free */
        let ret: c_int = unsafe { xsk_umem__delete(self.umem) };

        ret
    }

    pub fn send(&mut self, packets: &mut Vec<Packet>) -> u32 {
        /* free packet metadata and UMEM chunks as much as the number of filled slots in cq. */
        let mut cq_idx: u32 = 0;
        let filled: u32 =
            unsafe { xsk_ring_cons__peek(&mut self.cq, packets.len() as u32, &mut cq_idx) }; // fetch the number of filled slots(the number of packets completely sent) in cq

        if filled > 0 {
            for _ in 0..filled {
                unsafe {
                    self.free_idx(*xsk_ring_cons__comp_addr(&self.cq, cq_idx));
                } // free UMEM chunks as much as the number of sent packets (same as **filled)
                cq_idx += 1;
            }
            unsafe {
                xsk_ring_cons__release(&mut self.cq, filled);
            } // notify kernel that cq has empty slots with **filled (Dequeue)
        }

        /* reserve TX ring as much as batch_size before sending packets. */
        let mut tx_idx: u32 = 0;
        let reserved: u32 =
            unsafe { xsk_ring_prod__reserve(&mut self.tx, packets.len() as u32, &mut tx_idx) };

        /* If reservation of slots in TX ring is not sufficient, Don't send them and return 0. */
        if reserved < packets.len() as u32 {
            /* in case that kernel lost interrupt signal in the previous sending packets procedure,
            repeat to interrupt kernel to send packets which ketnel could have still held.
            (this procedure is for clearing filled slots in cq, so that cq can be reserved as much as **batch_size in the next execution of pv_send(). */
            unsafe {
                if xsk_ring_prod__needs_wakeup(&self.tx) != 0 {
                    libc::sendto(
                        xsk_socket__fd(self.xsk),
                        std::ptr::null::<libc::c_void>(),
                        0 as libc::size_t,
                        libc::MSG_DONTWAIT,
                        std::ptr::null::<libc::sockaddr>(),
                        0 as libc::socklen_t,
                    );
                }
            }
            return 0;
        }

        /* send packets if reservation of slots in TX ring is same as batch_size */
        for i in 0..reserved {
            let pkt_index: usize = (reserved - 1 - i) as usize;
            /* insert packets to be send into TX ring (Enqueue) */
            let tx_desc_ptr = unsafe { xsk_ring_prod__tx_desc(&mut self.tx, tx_idx + i) };
            unsafe {
                tx_desc_ptr.as_mut().unwrap().addr =
                    packets[pkt_index].private as u64 + packets[pkt_index].start as u64;
                tx_desc_ptr.as_mut().unwrap().len =
                    (packets[pkt_index].end - packets[pkt_index].start) as u32;
            }
            // packet_dump(&packets[pkt_index]);

            packets.pop(); // free packet metadata of sent packets.
        }

        unsafe {
            xsk_ring_prod__submit(&mut self.tx, reserved); // notify kernel of enqueuing TX ring as much as reserved.

            if xsk_ring_prod__needs_wakeup(&self.tx) != 0 {
                libc::sendto(
                    xsk_socket__fd(self.xsk),
                    std::ptr::null::<libc::c_void>(),
                    0 as libc::size_t,
                    libc::MSG_DONTWAIT,
                    std::ptr::null::<libc::sockaddr>(),
                    0 as libc::socklen_t,
                ); // interrupt kernel to send packets.
            }
        }

        reserved
    }

    pub fn receive(&mut self, packets: &mut Vec<Packet>) -> u32 {
        /* pre-allocate UMEM chunks into fq as much as **batch_size to receive packets */
        let mut fq_idx: u32 = 0;
        let reserved: u32 =
            unsafe { xsk_ring_prod__reserve(&mut self.fq, packets.capacity() as u32, &mut fq_idx) }; // reserve slots in fq as much as **batch_size.

        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(&mut self.fq, fq_idx + i) = self.alloc_idx();
            } // allocate UMEM chunks into fq.
        }
        unsafe {
            xsk_ring_prod__submit(&mut self.fq, reserved);
        } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

        /* save packet metadata from received packets in RX ring. */
        let mut rx_idx: u32 = 0;
        let mut received: u32 =
            unsafe { xsk_ring_cons__peek(&mut self.rx, packets.capacity() as u32, &mut rx_idx) }; // fetch the number of received packets in RX ring.

        if received > 0 {
            let mut metadata_count: u32 = 0;
            while metadata_count < received {
                /* create packet metadata */
                packets.push(Packet::new());
                let rx_desc_ptr: *const xdp_desc =
                    unsafe { xsk_ring_cons__rx_desc(&self.rx, rx_idx + metadata_count) }; // bringing information(packet address, packet length) of received packets through descriptors in RX ring

                /* save metadata */
                packets[metadata_count as usize].start = DEFAULT_HEADROOM;
                packets[metadata_count as usize].end =
                    DEFAULT_HEADROOM + unsafe { rx_desc_ptr.as_ref().unwrap().len as usize };
                packets[metadata_count as usize].buffer_size = self.chunk_size;
                packets[metadata_count as usize].buffer = unsafe {
                    xsk_umem__get_data(self.buffer, rx_desc_ptr.as_ref().unwrap().addr)
                        .cast::<u8>()
                        .sub(DEFAULT_HEADROOM)
                };
                packets[metadata_count as usize].private = unsafe {
                    (rx_desc_ptr.as_ref().unwrap().addr - DEFAULT_HEADROOM as u64) as *mut c_void
                };

                // packet_dump(&packets[metadata_count as usize]);
                metadata_count += 1;
            }

            unsafe {
                xsk_ring_cons__release(&mut self.rx, received);
            } // notify kernel of using filled slots in RX ring as much as **received

            if metadata_count != received {
                received = metadata_count;
            }
        }

        unsafe {
            if xsk_ring_prod__needs_wakeup(&self.fq) != 0 {
                libc::recvfrom(
                    xsk_socket__fd(self.xsk),
                    std::ptr::null_mut::<libc::c_void>(),
                    0 as libc::size_t,
                    libc::MSG_DONTWAIT,
                    std::ptr::null_mut::<libc::sockaddr>(),
                    std::ptr::null_mut::<u32>(),
                );
            }
        }

        received
    }
}
