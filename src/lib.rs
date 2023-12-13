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
use std::ffi::{CStr, CString};
use std::ptr::copy;
use std::rc::Rc;

use libc::strerror;

const DEFAULT_HEADROOM: usize = 256;

#[derive(Debug)]
struct ChunkPool {
    chunk_size: usize, // TODO: getter

    pool: Vec<u64>,

    pub buffer: *mut c_void, // buffer address.

    fq: xsk_ring_prod, // Fill queue
    cq: xsk_ring_cons, // Completion queue
    cq_size: usize,
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
pub struct Pool {
    chunk_size: usize,

    umem: *mut xsk_umem,
    chunk_pool: Rc<RefCell<ChunkPool>>,

    is_shared: bool,
}

#[derive(Debug)]
pub struct Nic {
    pub interface: NetworkInterface,
    xsk: *mut xsk_socket,

    chunk_pool: Rc<RefCell<ChunkPool>>,

    /* XSK rings */
    rx: xsk_ring_cons,
    tx: xsk_ring_prod,
}

struct ReservedResult {
    count: u32,
    idx: u32,
}

impl ChunkPool {
    fn new(
        chunk_size: usize,
        chunk_count: usize,
        buffer: *mut c_void,
        fq: xsk_ring_prod,
        cq: xsk_ring_cons,
        cq_size: usize,
    ) -> Self {
        /* initialize UMEM chunk information */
        let pool = (0..chunk_count)
            .map(|i| (i * chunk_size).try_into().unwrap())
            .collect::<Vec<u64>>();
        ChunkPool {
            chunk_size,
            pool,
            buffer,
            fq,
            cq,
            cq_size,
        }
    }

    fn alloc_addr(&mut self) -> Result<u64, &'static str> {
        if self.pool.is_empty() {
            Err("Chunk Pool is empty")
        } else {
            Ok(self.pool.pop().unwrap())
        }
    }

    fn free_addr(&mut self, chunk_addr: u64) {
        // Align
        let chunk_addr = chunk_addr - (chunk_addr % self.chunk_size as u64);
        self.pool.push(chunk_addr);
    }

    /// Reserve FQ and UMEM chunks as much as **len
    fn reserve_fq(&mut self, len: usize) -> Result<usize, &'static str> {
        let mut cq_idx = 0;

        let reserved = unsafe { xsk_ring_prod__reserve(&mut self.fq, len as u32, &mut cq_idx) };

        // Allocate UMEM chunks into fq
        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(&mut self.fq, cq_idx + i) = self.alloc_addr()?;
            }
        }

        // Notify kernel of allocation UMEM chunks into fq as much as **reserved
        unsafe {
            xsk_ring_prod__submit(&mut self.fq, reserved);
        }

        Ok(reserved.try_into().unwrap())
    }

    /// Reserve for txq
    fn reserve_txq(
        &mut self,
        txq: &mut xsk_ring_prod,
        len: usize,
    ) -> Result<ReservedResult, &'static str> {
        let mut idx = 0;
        let count = unsafe { xsk_ring_prod__reserve(txq, len as u32, &mut idx) };

        Ok(ReservedResult { count, idx })
    }

    /// Free packet metadata and UMEM chunks as much as the # of filled slots in CQ
    fn release(&mut self, len: usize) -> Result<u32, String> {
        let mut cq_idx = 0;
        let count = unsafe {
            // Fetch the number of filled slots(the # of packets completely sent) in cq
            xsk_ring_cons__peek(&mut self.cq, len as u32, &mut cq_idx)
        };
        if count > 0 {
            // Notify kernel that cq has empty slots with **filled (Dequeue)
            unsafe {
                xsk_ring_cons__release(&mut self.cq, count);
            }
        }

        Ok(count)
    }

    fn recv(
        &mut self,
        chunk_pool_rc: &Rc<RefCell<Self>>,
        len: usize,
        xsk: &*mut xsk_socket,
        rx: &mut xsk_ring_cons,
    ) -> Vec<Packet> {
        if self.reserve_fq(len).is_err() {
            // Pass
        };
        let mut packets = Vec::<Packet>::with_capacity(len);

        let mut rx_idx = 0;
        let received = unsafe { xsk_ring_cons__peek(rx, len as u32, &mut rx_idx) };

        for i in 0..received {
            let mut packet = Packet::new(chunk_pool_rc);
            let rx_desc = unsafe { xsk_ring_cons__rx_desc(&*rx, rx_idx + i).as_ref().unwrap() };
            packet.start = DEFAULT_HEADROOM;
            packet.end = DEFAULT_HEADROOM + rx_desc.len as usize;
            packet.buffer_size = self.chunk_size;
            packet.buffer = unsafe {
                xsk_umem__get_data(self.buffer, rx_desc.addr)
                    .cast::<u8>()
                    .sub(DEFAULT_HEADROOM)
            };
            packet.private = (rx_desc.addr - DEFAULT_HEADROOM as u64) as *mut c_void;

            packets.push(packet);
        }

        unsafe {
            xsk_ring_cons__release(rx, received);
        }

        unsafe {
            if xsk_ring_prod__needs_wakeup(&self.fq) != 0 {
                libc::recvfrom(
                    xsk_socket__fd(*xsk),
                    std::ptr::null_mut::<libc::c_void>(),
                    0 as libc::size_t,
                    libc::MSG_DONTWAIT,
                    std::ptr::null_mut::<libc::sockaddr>(),
                    std::ptr::null_mut::<u32>(),
                );
            }
        }

        packets
    }

    fn send(
        &mut self,
        packets: &mut Vec<Packet>,
        xsk: &*mut xsk_socket,
        tx: &mut xsk_ring_prod,
    ) -> usize {
        self.release(self.cq_size).unwrap();
        let reserved = self.reserve_txq(tx, packets.len()).unwrap();

        println!("reserved: {}, {}", reserved.count, reserved.idx);

        for (i, pkt) in packets.iter().enumerate().take(reserved.count as usize) {
            // Insert packets to be sent into the TX ring (Enqueue)
            let tx_desc = unsafe {
                xsk_ring_prod__tx_desc(tx, reserved.idx + i as u32)
                    .as_mut()
                    .unwrap()
            };
            tx_desc.addr = pkt.private as u64 + pkt.start as u64;
            tx_desc.len = (pkt.end - pkt.start) as u32;
        }

        // NOTE: Dropping packet here will cause Already Borrowed error.
        // So, we should drain packets after this function call.
        // packets.drain(0..reserved.count as usize);

        unsafe {
            xsk_ring_prod__submit(&mut *tx, reserved.count);
            if xsk_ring_prod__needs_wakeup(tx) != 0 {
                // Interrupt the kernel to send packets
                libc::sendto(
                    xsk_socket__fd(*xsk),
                    std::ptr::null::<libc::c_void>(),
                    0 as libc::size_t,
                    libc::MSG_DONTWAIT,
                    std::ptr::null::<libc::sockaddr>(),
                    0 as libc::socklen_t,
                );
            }
        }

        reserved.count.try_into().unwrap()
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

    /// replace payload with new data
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

    /// get payload as Vec<u8>
    pub fn get_buffer_mut(&mut self) -> &mut [u8] {
        unsafe {
            std::slice::from_raw_parts_mut(
                self.buffer.offset(self.start.try_into().unwrap()),
                self.end - self.start,
            )
        }
    }

    /// Resize payload size
    pub fn resize(&mut self, size: usize) {
        self.end = self.start + size;
    }

    /// Dump packet payload as hex
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
        self.chunk_pool.borrow_mut().free_addr(self.private as u64);
    }
}

impl Pool {
    /// Create Umem
    /// chunk_size: size of chunk. ex. 2048
    /// chunk_count: number of chunks. ex. 1024
    /// fq_size: size of fill queue. ex. 1024
    /// cq_size: size of completion queue. ex. 1024
    pub fn new(
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
        is_shared: bool,
    ) -> Result<Self, String> {
        let umem_ptr = alloc_zeroed_layout::<xsk_umem>()?;
        let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;

        let mut umem = umem_ptr.cast::<xsk_umem>(); // umem is needed to be dealloc after using packetvisor library.
        let mut fq = unsafe { std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()) };
        let mut cq = unsafe { std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()) };

        let umem_buffer_size = chunk_size * chunk_count;
        let mmap_address = unsafe {
            libc::mmap(
                std::ptr::null_mut::<libc::c_void>(),
                umem_buffer_size,
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_PRIVATE | libc::MAP_ANONYMOUS,
                -1, // fd
                0,  // offset
            )
        };
        if mmap_address == libc::MAP_FAILED {
            return Err("Failed to allocate memory for UMEM.".to_string());
        }

        let umem_cfg = xsk_umem_config {
            fill_size: fq_size as u32,
            comp_size: cq_size as u32,
            frame_size: chunk_size as u32,
            frame_headroom: XSK_UMEM__DEFAULT_FRAME_HEADROOM,
            flags: XSK_UMEM__DEFAULT_FLAGS,
        };

        let ret = unsafe {
            xsk_umem__create(
                &mut umem,
                mmap_address,
                umem_buffer_size as u64,
                &mut fq,
                &mut cq,
                &umem_cfg,
            )
        };

        if ret != 0 {
            unsafe {
                // Unmap first
                libc::munmap(mmap_address, umem_buffer_size);
            }
            return Err(format!("Failed to create UMEM: {}", -ret));
        }

        let buffer_addr = mmap_address;

        let chunk_pool = ChunkPool::new(
            chunk_size,
            chunk_count,
            buffer_addr,
            fq,
            cq,
            cq_size,
        );

        let mut obj = Self {
            chunk_size,
            umem,
            chunk_pool: Rc::new(RefCell::new(chunk_pool)),
            is_shared,
        };

        obj.pre_alloc(fq_size)?;

        Ok(obj)
    }

    /// Pre-allocate UMEM chunks into fill queue
    /// Use when attach a NIC
    fn pre_alloc(&mut self, fq_size: usize) -> Result<(), &'static str> {
        /* pre-allocate UMEM chunks into fq */
        let mut fq = self.chunk_pool.borrow_mut().fq;
        let mut fq_idx: u32 = 0;

        let reserved: u32 = unsafe { xsk_ring_prod__reserve(&mut fq, fq_size as u32, &mut fq_idx) };

        for i in 0..fq_size as u32 {
            let idx = self.alloc_addr()?;
            unsafe {
                *xsk_ring_prod__fill_addr(&mut fq, fq_idx + i) = idx;
            } // allocation of UMEM chunks into fq.
        }

        unsafe {
            xsk_ring_prod__submit(&mut fq, reserved);
        } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

        Ok(())
    }

    fn alloc_addr(&mut self) -> Result<u64, &'static str> {
        self.chunk_pool.borrow_mut().alloc_addr()
    }

    /// Allocate packet from UMEM
    pub fn try_alloc_packet(&mut self) -> Option<Packet> {
        match self.alloc_addr() {
            Err(_) => None,
            Ok(idx) => {
                let mut packet: Packet = Packet::new(&self.chunk_pool);
                packet.start = DEFAULT_HEADROOM;
                packet.end = DEFAULT_HEADROOM;
                packet.buffer_size = self.chunk_size;
                packet.buffer =
                    unsafe { xsk_umem__get_data(self.chunk_pool.borrow().buffer, idx) as *mut u8 };
                packet.private = idx as *mut c_void;

                Some(packet)
            }
        }
    }

    fn is_shared(&self) -> bool {
        self.is_shared
    }
}

impl Nic {
    pub fn new(if_name: &str, pool: &Pool, tx_size: usize, rx_size: usize) -> Result<Nic, String> {
        let interface = interfaces()
            .into_iter()
            .find(|elem| elem.name.as_str() == if_name)
            .ok_or(format!("Interface {} not found.", if_name))?;

        let xsk_ptr = alloc_zeroed_layout::<xsk_socket>()?;
        let rx_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
        let tx_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;

        let mut nic: Nic = unsafe {
            Nic {
                interface: interface.clone(),
                xsk: xsk_ptr.cast::<xsk_socket>(),
                chunk_pool: pool.chunk_pool.clone(),
                rx: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                tx: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
            }
        };
        match Nic::open(&mut nic, pool, tx_size, rx_size) {
            Ok(_) => Ok(nic),
            Err(e) => {
                // FIXME: Print here is fine. But segfault happened when printing in the caller.
                eprintln!("Failed to open NIC: {}", e);
                Err(e)
            }
        }
    }

    fn open(
        &mut self,
        pool: &Pool,
        rx_ring_size: usize,
        tx_ring_size: usize,
    ) -> Result<(), String> {
        /* setting xsk, RX ring, TX ring configuration */
        let xsk_cfg: xsk_socket_config = xsk_socket_config {
            rx_size: rx_ring_size.try_into().unwrap(),
            tx_size: tx_ring_size.try_into().unwrap(),
            /* zero means loading default XDP program.
            if you need to load other XDP program, set 1 on this flag and use xdp_program__open_file(), xdp_program__attach() in libxdp. */
            __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 { libxdp_flags: 0 },
            xdp_flags: XDP_FLAGS_DRV_MODE,
            bind_flags: XDP_USE_NEED_WAKEUP as u16,
        };
        let if_name = CString::new(self.interface.name.clone()).unwrap();
        let if_ptr = if_name.as_ptr() as *const c_char;
        /* create xsk socket */
        let mut chunk_pool = pool.chunk_pool.borrow_mut();
        let ret: c_int = unsafe {
            if pool.is_shared() {
                xsk_socket__create_shared(
                    &mut self.xsk,
                    if_ptr,
                    // self.interface.name.as_ptr().clone() as *const c_char,
                    0,
                    pool.umem,
                    &mut self.rx,
                    &mut self.tx,
                    &mut chunk_pool.fq,
                    &mut chunk_pool.cq,
                    &xsk_cfg,
                )
            } else {
                xsk_socket__create(
                    &mut self.xsk,
                    if_ptr,
                    // self.interface.name.as_ptr().clone() as *const c_char,
                    0,
                    pool.umem,
                    &mut self.rx,
                    &mut self.tx,
                    &xsk_cfg,
                )
            }
        };
        if ret != 0 {
            let msg = unsafe {
                CStr::from_ptr(strerror(-ret))
                    .to_string_lossy()
                    .into_owned()
            };
            let message = format!("Error: {}", msg);
            return Err(format!("xsk_socket__create failed: {}", message));
        }
        Ok(())
    }

    /// Send packets using NIC
    /// # Arguments
    /// * `packets` - Packets to send
    /// Sent packets are removed from the vector.
    /// # Returns
    /// Number of packets sent
    pub fn send(&mut self, packets: &mut Vec<Packet>) -> usize {
        let sent_count = self
            .chunk_pool
            .borrow_mut()
            .send(packets, &self.xsk, &mut self.tx);
        packets.drain(0..sent_count);

        sent_count
    }

    /// Receive packets using NIC
    /// # Arguments
    /// * `len` - Number of packets to receive
    /// # Returns
    /// Received packets
    pub fn receive(&mut self, len: usize) -> Vec<Packet> {
        self.chunk_pool
            .borrow_mut()
            .recv(&self.chunk_pool, len, &self.xsk, &mut self.rx)
    }
}

impl Drop for Nic {
    // move ownership of nic
    fn drop(&mut self) {
        // xsk delete
        unsafe {
            xsk_socket__delete(self.xsk);
        }
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
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
        Err("failed to allocate memory".to_string())
    } else {
        Ok(ptr)
    }
}
