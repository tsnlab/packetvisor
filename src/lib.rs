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
use std::collections::hash_set::HashSet;
use std::ffi::{CStr, CString};
use std::ptr::copy;
use std::rc::Rc;
use std::thread;
use std::time::Duration;

use libc::strerror;

const DEFAULT_HEADROOM: usize = 256;

/********************************************************************
 *
 * Global Variable (for Singleton-Pattern)
 *
 *******************************************************************/
static mut POOL: Option<Pool> = None;

/********************************************************************
 *
 * Structures
 *
 *******************************************************************/
#[derive(Debug)]
struct BufferPool {
    chunk_size: usize, // TODO: getter
    chunk_count: usize,

    pool: HashSet<u64>,

    pub buffer: *mut c_void, // buffer address.

    fq_size: usize,
    cq_size: usize,
}

#[derive(Debug)]
struct Pool {
    chunk_size: usize,

    umem: *mut xsk_umem,
    buffer_pool: Rc<RefCell<BufferPool>>,

    umem_fq: xsk_ring_prod,
    umem_cq: xsk_ring_cons,

    refcount: usize,
}

#[derive(Debug)]
pub struct Nic {
    pub interface: NetworkInterface,
    xsk: *mut xsk_socket,

    /* XSK rings */
    rxq: xsk_ring_cons,
    txq: xsk_ring_prod,
    umem_fq: xsk_ring_prod,
    umem_cq: xsk_ring_cons,
}

#[derive(Debug)]
pub struct Packet {
    pub start: usize,       // payload offset pointing the start of payload. ex. 256
    pub end: usize,         // payload offset point the end of payload. ex. 1000
    pub buffer_size: usize, // total size of buffer. ex. 2048
    pub buffer: *mut u8,    // buffer address.
    private: *mut c_void,   // DO NOT touch this.

    buffer_pool: Rc<RefCell<BufferPool>>,
}

struct ReservedResult {
    count: u32,
    idx: u32,
}

/********************************************************************
 *
 * Implementation
 *
 *******************************************************************/
impl BufferPool {
    fn new(
        chunk_size: usize,
        chunk_count: usize,
        buffer: *mut c_void,
        fq_size: usize,
        cq_size: usize,
    ) -> Self {
        /* initialize UMEM chunk information */
        let pool = (0..chunk_count)
            .map(|i| (i * chunk_size).try_into().unwrap())
            .collect::<HashSet<u64>>();
        Self {
            chunk_size,
            chunk_count,
            pool,
            buffer,
            fq_size,
            cq_size,
        }
    }

    fn alloc_addr(&mut self) -> Result<u64, &'static str> {
        if let Some(addr) = self.pool.iter().next() {
            let addr = *addr;
            self.pool.remove(&addr);
            Ok(addr)
        } else {
            Err("Chunk Pool is empty")
        }
    }

    fn free_addr(&mut self, chunk_addr: u64) {
        // Align
        let chunk_addr = chunk_addr - (chunk_addr % self.chunk_size as u64);

        #[cfg(debug_assertions)]
        if self.pool.contains(&chunk_addr) {
            eprintln!("Chunk Pool already contains chunk_addr: {}", chunk_addr);
        }

        self.pool.insert(chunk_addr);

        #[cfg(debug_assertions)]
        if self.pool.len() > self.chunk_count {
            eprintln!("Chunk Pool is overflowed");
        }
    }

    /// Reserve FQ and UMEM chunks as much as **len
    fn reserve_fq(&mut self, fq: &mut xsk_ring_prod, len: usize) -> Result<usize, &'static str> {
        let mut cq_idx = 0;
        let reserved = unsafe { xsk_ring_prod__reserve(fq, len as u32, &mut cq_idx) };

        // Allocate UMEM chunks into fq
        for i in 0..reserved {
            unsafe {
                *xsk_ring_prod__fill_addr(fq, cq_idx + i) = self.alloc_addr()?;
            }
        }

        // Notify kernel of allocation UMEM chunks into fq as much as **reserved
        unsafe {
            xsk_ring_prod__submit(fq, reserved);
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
    fn release(&mut self, cq: &mut xsk_ring_cons, len: usize) -> Result<u32, String> {
        let mut cq_idx = 0;
        let count = unsafe {
            // Fetch the number of filled slots(the # of packets completely sent) in cq
            xsk_ring_cons__peek(cq, len as u32, &mut cq_idx)
        };
        if count > 0 {
            // Notify kernel that cq has empty slots with **filled (Dequeue)
            unsafe {
                xsk_ring_cons__release(cq, count);
            }
        }

        Ok(count)
    }

    fn recv(
        &mut self,
        chunk_pool_rc: &Rc<RefCell<Self>>,
        len: usize,
        _xsk: &*mut xsk_socket,
        rxq: &mut xsk_ring_cons,
        fq: &mut xsk_ring_prod,
    ) -> Vec<Packet> {
        let mut packets = Vec::<Packet>::with_capacity(len);

        let mut rx_idx = 0;
        let received = unsafe { xsk_ring_cons__peek(rxq, len as u32, &mut rx_idx) };

        if received == 0 {
            return packets;
        }

        for i in 0..received {
            let mut packet = Packet::new(chunk_pool_rc);
            let rx_desc = unsafe { xsk_ring_cons__rx_desc(&*rxq, rx_idx + i).as_ref().unwrap() };
            packet.end += rx_desc.len as usize;
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
            xsk_ring_cons__release(rxq, received);
        }

        self.reserve_fq(fq, packets.len()).unwrap();

        /*
         * XSK manages interrupts through xsk_ring_prod__needs_wakup().
         *
         * If Packetvisor is assigned to core indexes [0] or [1], the Rx interrupt does not work properly.
         * This significantly degrades Packetvisor performance.
         * To resolve this issue, the interrupt is woken up whenever Recv() is called.
         */
        unsafe {
            if xsk_ring_prod__needs_wakeup(&*fq) != 0 {
                libc::recvfrom(
                    xsk_socket__fd(*_xsk),
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
        cq: &mut xsk_ring_cons,
    ) -> usize {
        self.release(cq, self.cq_size).unwrap();
        let reserved = self.reserve_txq(tx, packets.len()).unwrap();

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

impl Pool {
    fn new() -> Result<Self, String> {
        let umem_ptr = alloc_zeroed_layout::<xsk_umem>()?;
        let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;

        let umem = umem_ptr.cast::<xsk_umem>(); // umem is needed to be dealloc after using packetvisor library.
        let fq = unsafe { std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()) };
        let cq = unsafe { std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()) };

        let chunk_pool = BufferPool::new(0, 0, std::ptr::null_mut(), 0, 0);

        let chunk_size = 0;
        let refcount = 0;

        let obj = Self {
            chunk_size,
            umem,
            buffer_pool: Rc::new(RefCell::new(chunk_pool)),
            umem_fq: fq,
            umem_cq: cq,
            refcount,
        };

        Ok(obj)
    }

    fn init(
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
    ) -> Result<(), String> {
        match unsafe { POOL.as_ref() } {
            None => {
                let mut pool_obj = Pool::new()?;
                pool_obj.re_init(chunk_size, chunk_count, fq_size, cq_size)?;
                unsafe { POOL = Some(pool_obj) };
            }
            _ => {
                #[cfg(debug_assertions)]
                eprintln!("Pool has already been initialized.");
            }
        }

        Ok(())
    }

    fn re_init(
        &mut self,
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
    ) -> Result<(), String> {
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
                &mut self.umem,
                mmap_address,
                umem_buffer_size as u64,
                &mut self.umem_fq,
                &mut self.umem_cq,
                &umem_cfg,
            )
        };

        if ret != 0 {
            unsafe {
                // Unmap first
                libc::munmap(mmap_address, umem_buffer_size);
            }
            let msg = unsafe {
                CStr::from_ptr(strerror(-ret))
                    .to_string_lossy()
                    .into_owned()
            };

            return Err(format!("Failed to create UMEM: {}", msg));
        }

        let chunk_pool = BufferPool::new(chunk_size, chunk_count, mmap_address, fq_size, cq_size);
        let mut borrow_buffer_pool = self.buffer_pool.borrow_mut();
        *borrow_buffer_pool = chunk_pool;

        self.chunk_size = chunk_size;

        Ok(())
    }

    fn alloc_addr(&mut self) -> Result<u64, &'static str> {
        self.buffer_pool.borrow_mut().alloc_addr()
    }

    /// Allocate packet from UMEM
    fn try_alloc_packet(&mut self) -> Option<Packet> {
        match self.alloc_addr() {
            Err(_) => None,
            Ok(idx) => {
                let mut packet: Packet = Packet::new(&self.buffer_pool);
                packet.buffer_size = self.chunk_size;
                packet.buffer =
                    unsafe { xsk_umem__get_data(self.buffer_pool.borrow().buffer, idx) as *mut u8 };
                packet.private = idx as *mut c_void;

                Some(packet)
            }
        }
    }
}

impl Nic {
    pub fn new(
        if_name: &str,
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
        tx_size: usize,
        rx_size: usize,
    ) -> Result<Nic, String> {
        let interface = interfaces()
            .into_iter()
            .find(|elem| elem.name.as_str() == if_name)
            .ok_or(format!("Interface {} not found.", if_name))?;

        let xsk_ptr = alloc_zeroed_layout::<xsk_socket>()?;
        let rx_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
        let tx_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
        let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;

        /* The result of Pool::init() must be unwrapped using the unwrap() function. \
         * If you do not use unwrap(), the internal fields of the Pool object will not \
         * be properly initialized, which can lead to potential problems.
         *
         * Ex) Fallback between XSK's SKB and DRV modes may not be possible. \
         *     Other unexpected problems may occur. */
        Pool::init(chunk_size, chunk_count, fq_size, cq_size).unwrap();

        let mut nic = unsafe {
            Nic {
                interface: interface.clone(),
                xsk: xsk_ptr.cast::<xsk_socket>(),
                rxq: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                txq: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
                umem_fq: std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()),
                umem_cq: std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()),
            }
        };

        match Nic::open(
            &mut nic,
            chunk_size,
            chunk_count,
            fq_size,
            cq_size,
            tx_size,
            rx_size,
        ) {
            Ok(_) => {
                unsafe {
                    POOL.as_mut().unwrap().refcount += 1;
                };
                Ok(nic)
            }
            Err(e) => {
                // FIXME: Print here is fine. But segfault happened when printing in the caller.
                eprintln!("Failed to open NIC: {}", e);
                Err(e)
            }
        }
    }

    fn open(
        &mut self,
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
        rx_ring_size: usize,
        tx_ring_size: usize,
    ) -> Result<(), String> {
        let mut xsk_cfg: xsk_socket_config = xsk_socket_config {
            rx_size: rx_ring_size.try_into().unwrap(),
            tx_size: tx_ring_size.try_into().unwrap(),
            __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 { libxdp_flags: 0 },
            xdp_flags: XDP_FLAGS_DRV_MODE,
            bind_flags: XDP_USE_NEED_WAKEUP as u16,
        };
        let if_name = CString::new(self.interface.name.clone()).unwrap();
        let if_ptr = if_name.as_ptr() as *const c_char;

        let ret: c_int = unsafe {
            xsk_socket__create_shared(
                &mut self.xsk,
                if_ptr,
                0,
                POOL.as_mut().unwrap().umem,
                &mut self.rxq,
                &mut self.txq,
                &mut self.umem_fq,
                &mut self.umem_cq,
                &xsk_cfg,
            )
        };

        if ret != 0 {
            match unsafe { POOL.as_ref().unwrap().refcount } {
                0 => {
                    // Pool Full-Fallback
                    unsafe { xsk_umem__delete(POOL.as_mut().unwrap().umem) };
                    thread::sleep(Duration::from_millis(100));
                    unsafe {
                        POOL.as_mut()
                            .unwrap()
                            .re_init(chunk_size, chunk_count, fq_size, cq_size)?
                    };
                }
                refcount if refcount > 0 => {
                    // Pool Semi-Fallback
                    thread::sleep(Duration::from_millis(100));
                }
                _ => {
                    return Err("Pool Fallback failed".to_string());
                }
            }

            xsk_cfg.xdp_flags = XDP_FLAGS_SKB_MODE;
            let ret: c_int = unsafe {
                xsk_socket__create_shared(
                    &mut self.xsk,
                    if_ptr,
                    0,
                    POOL.as_mut().unwrap().umem,
                    &mut self.rxq,
                    &mut self.txq,
                    &mut POOL.as_mut().unwrap().umem_fq,
                    &mut POOL.as_mut().unwrap().umem_cq,
                    &xsk_cfg,
                )
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
        }

        /*
         * After calling xsk_umem__create(), the fill_q and comp_q of the UMEM are initialized.
         * These are assigned to the first XSK through xsk_socket__create() or  _shared().
         * However, this does not work properly in Rust.
         *
         * Therefore, we stored the fill_q and comp_q in the Pool object before calling xsk_umem__create().
         * After xsk_socket__create() or _shared() finishes, the stored fill_q and comp_q are assigned to the Nic object.
         * This resolves the Segmentation Fault issue.
         */
        if unsafe { POOL.as_ref().unwrap().refcount == 0 } {
            self.umem_fq = unsafe { POOL.as_ref().unwrap().umem_fq };
            self.umem_cq = unsafe { POOL.as_ref().unwrap().umem_cq };

            let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
            let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
            unsafe {
                POOL.as_mut().unwrap().umem_fq = std::ptr::read(fq_ptr.cast::<xsk_ring_prod>());
                POOL.as_mut().unwrap().umem_cq = std::ptr::read(cq_ptr.cast::<xsk_ring_cons>());
            };
        }

        let fq_size = unsafe { POOL.as_mut().unwrap().buffer_pool.borrow().fq_size };
        unsafe {
            POOL.as_mut()
                .unwrap()
                .buffer_pool
                .borrow_mut()
                .reserve_fq(&mut self.umem_fq, fq_size)?
        };

        Ok(())
    }

    /// Allocate packet using Pool
    pub fn alloc_packet(&self) -> Option<Packet> {
        unsafe { POOL.as_mut().unwrap().try_alloc_packet() }
    }

    /// Send packets using NIC
    /// # Arguments
    /// * `packets` - Packets to send
    /// Sent packets are removed from the vector.
    /// # Returns
    /// Number of packets sent
    pub fn send(&mut self, packets: &mut Vec<Packet>) -> usize {
        let sent_count = unsafe {
            POOL.as_mut().unwrap().buffer_pool.borrow_mut().send(
                packets,
                &self.xsk,
                &mut self.txq,
                &mut self.umem_cq,
            )
        };
        packets.drain(0..sent_count);

        sent_count
    }

    /// Receive packets using NIC
    /// # Arguments
    /// * `len` - Number of packets to receive
    /// # Returns
    /// Received packets
    pub fn receive(&mut self, len: usize) -> Vec<Packet> {
        unsafe {
            POOL.as_mut().unwrap().buffer_pool.borrow_mut().recv(
                &POOL.as_mut().unwrap().buffer_pool,
                len,
                &self.xsk,
                &mut self.rxq,
                &mut self.umem_fq,
            )
        }
    }
}

impl Packet {
    fn new(chunk_pool: &Rc<RefCell<BufferPool>>) -> Packet {
        Packet {
            start: DEFAULT_HEADROOM,
            end: DEFAULT_HEADROOM,
            buffer_size: 0,
            buffer: std::ptr::null_mut(),
            private: std::ptr::null_mut(),
            buffer_pool: chunk_pool.clone(),
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
    pub fn resize(&mut self, size: usize) -> Result<(), String> {
        if self.buffer_size < self.start + size {
            self.start -= size;
        }
        if (self.start as isize) < (0_isize) {
            return Err("The requested size is to large.".to_string());
        }
        self.end = self.start + size;

        Ok(())
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

/********************************************************************
 *
 * Drop
 *
 *******************************************************************/
impl Drop for Pool {
    fn drop(&mut self) {
        // Free UMEM
        let ret: c_int = unsafe { xsk_umem__delete(self.umem) };
        if ret != 0 {
            eprintln!("failed to free umem");
        }
    }
}

impl Drop for Nic {
    // move ownership of nic
    fn drop(&mut self) {
        // xsk delete
        unsafe {
            xsk_socket__delete(self.xsk);
        }

        unsafe {
            POOL.as_mut().unwrap().refcount -= 1;
        };
    }
}

impl Drop for Packet {
    fn drop(&mut self) {
        self.buffer_pool.borrow_mut().free_addr(self.private as u64);
    }
}

/********************************************************************
 *
 * Other functions
 *
 *******************************************************************/
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
