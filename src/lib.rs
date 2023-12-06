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
    pub chunk_count: usize,
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

    umem_rc: Rc<RefCell<Umem>>,

    /* XSK rings */
    rx: xsk_ring_cons,
    tx: xsk_ring_prod,
}

pub struct ReservedResult {
    pub count: u32,
    pub idx: u32,
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
            buffer: std::ptr::null_mut(), // Initialized later
            chunk_count,
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
        self.configure_umem(fq_size, cq_size)?;

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

    fn configure_umem(&mut self, fq_size: usize, cq_size: usize) -> Result<(), String> {
        let umem_buffer_size = self.chunk_count * self.chunk_size;
        let mmap_address = unsafe {
            libc::mmap(
                std::ptr::null_mut::<libc::c_void>(),
                umem_buffer_size,
                libc::PROT_READ | libc::PROT_WRITE,
                libc::MAP_PRIVATE | libc::MAP_ANONYMOUS,
                -1, // fd
                0, // offset
            )
        };
        if mmap_address == libc::MAP_FAILED {
            return Err(String::from("Failed to allocate memory for UMEM."));
        }

        let umem_cfg = xsk_umem_config {
            fill_size: fq_size as u32,
            comp_size: cq_size as u32,
            frame_size: self.chunk_size as u32,
            frame_headroom: XSK_UMEM__DEFAULT_FRAME_HEADROOM,
            flags: XSK_UMEM__DEFAULT_FLAGS,
        };

        let ret = unsafe {
            xsk_umem__create(
                &mut self.umem,
                mmap_address,
                umem_buffer_size as u64,
                &mut self.fq,
                &mut self.cq,
                &umem_cfg,
            )
        };

        if ret != 0 {
            unsafe {
                // Unmap first
                libc::munmap(mmap_address, umem_buffer_size);
            }
            return Err(format!("Failed to create UMEM: {}", ret));
        }

        self.buffer = mmap_address;
        Ok(())
    }

    fn alloc_idx(&mut self) -> u64 {
        self.chunk_pool.borrow_mut().pop()
    }

    pub fn alloc_packet(&mut self) -> Option<Packet> {
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

    /// Reserve packet metadata and UMEM chunks as much as **len
    fn reserve(&mut self, len: usize) -> Result<ReservedResult, String> {
        let mut idx = 0;
        let reserved = unsafe {
            xsk_ring_prod__reserve(&mut self.fq, len as u32, &mut idx)
        };

        // Allocate UMEM chunks into fq
        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(&mut self.fq, idx + i) = self.alloc_idx();
            }
        }

        // Notify kernel of allocation UMEM chunks into fq as much as **reserved
        unsafe {
            xsk_ring_prod__submit(&mut self.fq, reserved);
        }

        Ok(ReservedResult {
            count: reserved,
            idx,
        })
    }

    /// Free packet metadata and UMEM chunks as much as the # of filled slots in CQ
    fn fill(&mut self, len: usize) -> Result<u32, String> {
        let mut cq_idx = 0;
        let filled = unsafe {
            // Fetch the number of filled slots(the # of packets completely sent) in cq
            xsk_ring_cons__peek(&mut self.cq, len as u32, &mut cq_idx)
        };
        if filled > 0 {
            // Notify kernel that cq has empty slots with **filled (Dequeue)
            unsafe {
                xsk_ring_cons__release(&mut self.cq, filled);
            }
        }

        Ok(filled)
    }

    fn recv(&mut self, len: usize, xsk: &*mut xsk_socket, rx: &mut xsk_ring_cons) -> Vec<Packet> {
        if self.reserve(len).is_err() {
            // Pass
        };
        let mut packets = Vec::<Packet>::with_capacity(len);

        let mut rx_idx = 0;
        let received = unsafe {
            xsk_ring_cons__peek(rx, len as u32, &mut rx_idx)
        };

        for i in 0..received {
            let mut packet = Packet::new(&self.chunk_pool);
            let rx_desc_ptr = unsafe {
                xsk_ring_cons__rx_desc(&*rx, rx_idx + i)
            };
            packet.start = DEFAULT_HEADROOM;
            packet.end = DEFAULT_HEADROOM + unsafe { rx_desc_ptr.as_ref().unwrap().len as usize };
            packet.buffer_size = self.chunk_size;
            packet.buffer = unsafe {
                xsk_umem__get_data(self.buffer, rx_desc_ptr.as_ref().unwrap().addr)
                    .cast::<u8>()
                    .sub(DEFAULT_HEADROOM)
            };
            packet.private = unsafe {
                (rx_desc_ptr.as_ref().unwrap().addr - DEFAULT_HEADROOM as u64) as *mut c_void
            };

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

    fn send(&mut self, packets: &mut Vec<Packet>, xsk: &*mut xsk_socket, tx: &mut xsk_ring_prod) -> usize {
        let _filled = self.fill(packets.len()).unwrap();
        let reserved = self.reserve(packets.len()).unwrap();

        // println!("reserved: {}, {}", reserved.count, reserved.idx);

        for (i, pkt) in packets.iter().enumerate().take(reserved.count as usize) {
            // Insert packets to be sent into the TX ring (Enqueue)
            let tx_desc_ptr = unsafe {
                xsk_ring_prod__tx_desc(tx, reserved.idx + i as u32)
            };
            unsafe {
                let tx_desc = tx_desc_ptr.as_mut().unwrap();
                tx_desc.addr = pkt.private as u64 + pkt.start as u64;
                tx_desc.len = (pkt.end - pkt.start) as u32;
            }
        }

        packets.drain(0..reserved.count as usize);

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

impl NIC {
    pub fn new(
        if_name: &str,
        umem_rc: Rc<RefCell<Umem>>,
        tx_size: usize,
        rx_size: usize,
    ) -> Result<NIC, String> {
        let interface = interfaces()
            .into_iter()
            .find(|elem| elem.name.as_str() == if_name)
            .ok_or(format!("Interface {} not found.", if_name))?;

        let xsk_ptr = alloc_zeroed_layout::<xsk_socket>()?;
        let rx_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
        let tx_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;

        let mut nic: NIC = unsafe {
            NIC {
                interface: interface.clone(),
                xsk: xsk_ptr.cast::<xsk_socket>(),
                umem_rc,
                rx: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                tx: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
            }
        };
        nic.open(tx_size, rx_size)?;
        Ok(nic)
    }

    fn open(
        &mut self,
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
        let ret: c_int = unsafe {
            xsk_socket__create(
                &mut self.xsk,
                if_ptr,
                // self.interface.name.as_ptr().clone() as *const c_char,
                0,
                self.umem_rc.borrow_mut().umem,
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

    pub fn copy_from(&mut self, src: &Packet) -> Option<Packet> {
        let len = src.end - src.start;

        if let Some(mut packet) = self.alloc_packet() {
            unsafe {
                std::ptr::copy_nonoverlapping(
                    src.buffer.add(src.start),
                    packet.buffer.add(packet.start),
                    len,
                );
            }
            packet.end = packet.start + len;
            self.umem_rc.borrow_mut().chunk_pool.borrow_mut().push(src.private as u64);
            Some(packet)
        } else {
            println!("Failed to add packet to NIC.");
            None
        }
    }

    pub fn alloc_packet(&mut self) -> Option<Packet> {
        self.umem_rc.borrow_mut().alloc_packet()
    }

    #[deprecated(note = "Free does nothing now. Packet is automatically freed when it is dropped.")]
    pub fn free(&mut self, _packet: &Packet) {}

    #[deprecated(note = "Close does nothing now. Socket is automatically closed when it is dropped.")]
    pub fn close(self) -> c_int {
        0
    }

    /// Send packets using NIC
    /// # Arguments
    /// * `packets` - Packets to send
    /// Sent packets are removed from the vector.
    /// # Returns
    /// Number of packets sent
    pub fn send(&mut self, packets: &mut Vec<Packet>) -> usize {
        self.umem_rc.borrow_mut().send(packets, &self.xsk, &mut self.tx)
    }

    /// Receive packets using NIC
    /// # Arguments
    /// * `len` - Number of packets to receive
    /// # Returns
    /// Received packets
    pub fn receive(&mut self, len: usize) -> Vec<Packet> {
        self.umem_rc.borrow_mut().recv(len, &self.xsk, &mut self.rx)
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
    }
}

impl Drop for Umem {
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
