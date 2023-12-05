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
use std::cell::RefCell;
use std::ffi::CString;
use std::ptr::copy;
use std::rc::Rc;

const DEFAULT_HEADROOM: usize = 256;

#[derive(Debug)]
struct ChunkPool {
    chunk_size: usize,
    pool: Vec<u64>,
}

#[derive(Debug)]
pub struct Packet {
    pub start: usize,       // payload offset pointing the start of payload. ex. 256
    pub end: usize,         // payload offset point the end of payload. ex. 1000
    pub buffer_size: usize, // total size of buffer. ex. 2048
    pub buffer: *mut u8,    // buffer address.
    private: *mut c_void,   // DO NOT touch this.

    chunk_pool: Rc<RefCell<ChunkPool>>,
}

#[derive(Debug)]
pub struct Umem {
    pub chunk_size: usize,

    umem: *mut xsk_umem,
    buffer: *mut c_void,
    chunk_pool: Rc<RefCell<ChunkPool>>,

    // XSK rings
    fq: xsk_ring_prod, // Fill queue
    cq: xsk_ring_cons, // Completion queue
}

#[derive(Debug)]
pub struct NIC {
    pub interface: NetworkInterface,
    xsk: *mut xsk_socket,

    umem: *mut xsk_umem,
    buffer: *mut c_void,
    chunk_size: usize,

    chunk_pool: Rc<RefCell<ChunkPool>>,

    /* XSK rings */
    fq: xsk_ring_prod,
    rx: xsk_ring_cons,
    cq: xsk_ring_cons,
    tx: xsk_ring_prod,
}

impl ChunkPool {
    fn new(chunk_size: usize, capacity: usize) -> Self {
        ChunkPool {
            chunk_size,
            pool: Vec::with_capacity(capacity),
        }
    }

    fn pop(&mut self) -> u64 {
        match self.pool.pop() {
            Some(chunk_addr) => chunk_addr,
            None => u64::MAX,
        }
    }

    fn push(&mut self, chunk_addr: u64) {
        // align **chunk_addr with chunk size
        let remainder = chunk_addr % (self.chunk_size as u64);
        let chunk_addr = chunk_addr - remainder;
        self.pool.push(chunk_addr);
    }

    fn capacity(&self) -> usize {
        self.pool.capacity()
    }
}

impl Packet {
    fn new(chunk_pool: &Rc<RefCell<ChunkPool>>) -> Packet {
        Packet {
            start: 0,
            end: 0,
            buffer_size: 0,
            buffer: std::ptr::null_mut(),
            private: std::ptr::null_mut(),
            chunk_pool: chunk_pool.clone(),
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

        let length: usize = self.end - self.start;
        let mut count: usize = 0;

        unsafe {
            println!("---packet dump--- chunk addr: {}", chunk_address);

            loop {
                let read_offset: usize = count + self.start;
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

impl Drop for Packet {
    fn drop(&mut self) {
        self.chunk_pool.borrow_mut().push(self.private as u64);
    }
}

impl Umem {
    pub fn new(
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
    ) -> Result<Rc<RefCell<Self>>, String> {
        let umem_ptr = alloc_zeroed_layout::<xsk_umem>()?;
        let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;

        let mut obj = unsafe { Self {
            umem: umem_ptr.cast::<xsk_umem>(), // umem is needed to be dealloc after using packetvisor library.
            buffer: std::ptr::null_mut(),
            chunk_size,
            chunk_pool: Rc::new(RefCell::new(ChunkPool::new(
                chunk_size,
                chunk_count,
            ))),
            fq: std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()),
            cq: std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()),
        } };

        obj.initialize_umem(fq_size, cq_size)?;

        Ok(Rc::new(RefCell::new(obj)))
    }

    fn initialize_umem(&mut self, fq_size: usize, cq_size: usize) -> Result<(), String> {
        /* initialize UMEM chunk information */
        let capacity = self.chunk_pool.borrow().capacity();
        {
            let mut chunk_pool = self.chunk_pool.borrow_mut();
            for i in 0..capacity {
                chunk_pool.push((i * self.chunk_size) as u64); // Put chunk address
            }
        }

        /* configure UMEM */
        // let ret = self.configure_umem(
        //     self.chunk_size,
        //     capacity,
        //     filling_ring_size,
        //     completion_ring_size,
        // );
        // if ret.is_err() {
        //     return Err(format!("Failed to configure UMEM: {}", ret.unwrap_err()));
        // }

        /* pre-allocate UMEM chunks into fq */
        let mut fq_idx: u32 = 0;
        let reserved: u32 =
            unsafe { xsk_ring_prod__reserve(&mut self.fq, fq_size as u32, &mut fq_idx) };

        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(&mut self.fq, fq_idx + i) = self.alloc_idx();
            } // allocation of UMEM chunks into fq.
        }
        unsafe {
            xsk_ring_prod__submit(&mut self.fq, reserved);
        } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

        Ok(())
    }

    fn alloc_idx(&mut self) -> u64 {
        self.chunk_pool.borrow_mut().pop()
    }

    pub fn alloc(&mut self) -> Option<Packet> {
        let idx: u64 = self.alloc_idx();

        match idx {
            u64::MAX => None,
            _ => {
                let mut packet: Packet = Packet::new(&self.chunk_pool);
                packet.start = DEFAULT_HEADROOM;
                packet.end = DEFAULT_HEADROOM;
                packet.buffer_size = self.chunk_size;
                packet.buffer = unsafe { xsk_umem__get_data(self.buffer, idx).cast::<u8>() };
                packet.private = idx as *mut c_void;

                Some(packet)
            }
        }
    }
}

impl NIC {
    pub fn new(
        if_name: &str,
        set_chunk_size: usize,
        set_chunk_count: usize,
    ) -> Result<NIC, String> {
        let interface = interfaces()
            .into_iter()
            .find(|elem| elem.name.as_str() == if_name)
            .ok_or(format!("Interface {} not found.", if_name))?;

        let xsk_ptr = alloc_zeroed_layout::<xsk_socket>()?;
        let umem_ptr = alloc_zeroed_layout::<xsk_umem>()?;
        let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let rx_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
        let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
        let tx_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;

        let nic: NIC = unsafe {
            NIC {
                interface: interface.clone(),
                xsk: xsk_ptr.cast::<xsk_socket>(),
                umem: umem_ptr.cast::<xsk_umem>(), // umem is needed to be dealloc after using packetvisor library.
                buffer: std::ptr::null_mut(),
                chunk_size: set_chunk_size,
                chunk_pool: Rc::new(RefCell::new(ChunkPool::new(
                    set_chunk_size,
                    set_chunk_count,
                ))),
                fq: std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()),
                rx: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                cq: std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()),
                tx: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
            }
        };
        Ok(nic)
    }

    pub fn copy_from(&mut self, src: &Packet) -> Option<Packet> {
        let len = src.end - src.start;

        if let Some(mut packet) = self.alloc() {
            unsafe {
                std::ptr::copy_nonoverlapping(
                    src.buffer.add(src.start),
                    packet.buffer.add(packet.start),
                    len,
                );
            }
            packet.end = packet.start + len;
            self.chunk_pool.borrow_mut().push(src.private as u64);
            Some(packet)
        } else {
            println!("Failed to add packet to NIC.");
            None
        }
    }

    fn alloc_idx(&mut self) -> u64 {
        self.chunk_pool.borrow_mut().pop()
    }

    pub fn alloc(&mut self) -> Option<Packet> {
        let idx: u64 = self.alloc_idx();

        match idx {
            u64::MAX => None,
            _ => {
                let mut packet: Packet = Packet::new(&self.chunk_pool);
                packet.start = DEFAULT_HEADROOM;
                packet.end = DEFAULT_HEADROOM;
                packet.buffer_size = self.chunk_size;
                packet.buffer = unsafe { xsk_umem__get_data(self.buffer, idx).cast::<u8>() };
                packet.private = idx as *mut c_void;

                Some(packet)
            }
        }
    }

    #[deprecated(note = "Free does nothing now. Packet is automatically freed when it is dropped.")]
    pub fn free(&mut self, _packet: &Packet) {}

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
        let capacity = self.chunk_pool.borrow().capacity();
        for i in 0..capacity {
            self.chunk_pool
                .borrow_mut()
                .push((i * self.chunk_size) as u64); // put chunk address
        }

        /* configure UMEM */
        let capacity = self.chunk_pool.borrow().capacity();
        let ret = self.configure_umem(
            self.chunk_size,
            capacity,
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
            xdp_flags: XDP_FLAGS_DRV_MODE,
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

    #[deprecated(note = "Close does nothing now. Socket is automatically closed when it is dropped.")]
    pub fn close(self) -> c_int {
        0
    }

    pub fn send(&mut self, packets: &mut Vec<Packet>) -> u32 {
        /* free packet metadata and UMEM chunks as much as the number of filled slots in cq. */
        let mut cq_idx: u32 = 0;
        let filled: u32 =
            unsafe { xsk_ring_cons__peek(&mut self.cq, packets.len() as u32, &mut cq_idx) }; // fetch the number of filled slots(the number of packets completely sent) in cq

        if filled > 0 {
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
            let pkt = &packets[i as usize];
            /* insert packets to be send into TX ring (Enqueue) */
            let tx_desc_ptr = unsafe { xsk_ring_prod__tx_desc(&mut self.tx, tx_idx + i) };
            unsafe {
                let tx_desc = tx_desc_ptr.as_mut().unwrap();
                tx_desc.addr = pkt.private as u64 + pkt.start as u64;
                tx_desc.len = (pkt.end - pkt.start) as u32;
            }
        }
        packets.drain(0..reserved as usize); // remove packets which have been sent from packets vector

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
                packets.push(Packet::new(&self.chunk_pool));
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

impl Drop for NIC {
    // move ownership of nic
    fn drop(&mut self) {
        println!("Drop NIC");
        // xsk delete
        unsafe {
            xsk_socket__delete(self.xsk);
        }

        // TODO: Not free umem
        // Free UMEM
        let ret: c_int = unsafe { xsk_umem__delete(self.umem) };
        if ret != 0 {
            eprintln!("failed to free umem");
        }
    }
}

fn alloc_zeroed_layout<T: 'static>() -> Result<*mut u8, String> {
    let ptr;
    unsafe {
        let layout = Layout::new::<T>();
        ptr = alloc_zeroed(layout);
    }
    if ptr.is_null() {
        Err(format!("failed to allocate memory"))
    } else {
        Ok(ptr)
    }
}
