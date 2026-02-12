//!
//! # Packetvisor
//!
//! `Packetvisor` is a Raw Packet I/O framework based on the Rust language.
//! It can process packets much faster than `Standard Sockets` through the
//! Linux Kernel's `eXpress Data Path (XDP)`.
//!
//! ## Key features
//!
//! **1. Bypassing the Network Stack in the Linux Kernel and Achieving Zero-Copy with `XDP`**
//! * Packetvisor completely bypasses the Network Stack in the Linux kernel using `XDP`,
//!   reducing unnecessary overhead in the packet processing.
//!
//! **2. High-Speed Packet Processing with the `pv::Nic` Structure**
//! * `Packetvisor` uses the `pv::Nic` structure to attach to specific Network Interface.
//!   This structure utilizes `XDP_SOCKET(XSK)` to directly process transmit
//!   and receive packets, allowing applications to directly transmit and
//!   receive packets in the application space. This helps to simplify the
//!   data processing process and improve performance.
//!
//! **3. Easy Development with Rust**
//! * `Packetvisor` is developed based on the Rust language, so you can use
//!   Rust to create high-performance _firewalls_ and implement _tunneling
//!   protocols_ with just a few dozen lines of code.
//!
//! **4. Automatically detects XDP mode**
//! * `Packetvisor` supports both `XDP` `Native(=DRV)` and `Generic(=SKB)` modes,
//!   and the mode selection is automatically determined by `Packetvisor`.
//!
//! ## Examples
//! Various examples for packet _echo_, _filtering_, _forwarding_, etc.
//! can be found in the [examples] directory.
//!
//! [examples]: https://github.com/tsnlab/packetvisor/tree/master/examples

mod bindings {
    #![allow(non_upper_case_globals)]
    #![allow(non_camel_case_types)]
    #![allow(non_snake_case)]
    #![allow(dead_code)]
    #![allow(clippy::all)]

    #[cfg(docsrs)]
    include!("bindings.rs");
    #[cfg(not(docsrs))]
    include!(concat!(env!("OUT_DIR"), "/bindings.rs"));
}

mod xdp_config;

use bindings::*;
use pnet::datalink::{interfaces, NetworkInterface};
use std::alloc::{alloc_zeroed, Layout};
use std::cell::RefCell;
use std::collections::hash_set::HashSet;
use std::convert::TryInto;
use std::env;
use std::ffi::{c_char, c_int, c_void, CStr, CString};
use std::ptr::copy;
use std::rc::Rc;
use std::thread;
use std::time::Duration;

use libc::strerror;

const DEFAULT_HEADROOM: usize = 256;

#[repr(C)]
#[derive(Clone, Copy, Debug, Default)]
pub struct AfXdpRxConfig {
    pub l2_flags: u32,
    pub l3_flags: u32,
    pub l4_flags: u32,
}

/********************************************************************
 *
 * Structures
 *
 *******************************************************************/
#[derive(Debug)]
struct BufferPool {
    chunk_size: usize,
    #[allow(dead_code)]
    chunk_count: usize,

    pool: HashSet<u64>,

    buffer: *mut c_void, // buffer address.

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

/// NIC Structure that supports Packetvisor
#[derive(Debug)]
pub struct Nic {
    /// Attached network interface information.
    /// (ex. `interface name`, `L2-3 address`, etc.)
    pub interface: NetworkInterface,
    xsk: *mut xsk_socket,

    /* XSK rings */
    rxq: xsk_ring_cons,
    txq: xsk_ring_prod,
    umem_fq: xsk_ring_prod,
    umem_cq: xsk_ring_cons,

    /* XDP program */
    xdp_prog: *mut xdp_program,
    xdp_attach_mode: xdp_attach_mode,
    ifindex: i32,
    xsks_map_fd: i32,
    config_map_fd: i32, // File descriptor for config_map to pass values to XDP program
}

/// Packet Structure used by Packetvisor
#[derive(Debug)]
pub struct Packet {
    /// payload offset from `buffer` pointing the start of payload.
    pub start: usize,
    /// payload offset from `buffer` point the end of payload.
    pub end: usize,
    /// total size of buffer.
    pub buffer_size: usize,
    /// buffer address.
    pub buffer: *mut u8,
    private: *mut c_void, // DO NOT touch this.

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
        packets: &mut [Packet],
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

    fn instance() -> *mut Self {
        static INSTANCE: std::sync::OnceLock<Pool> = std::sync::OnceLock::new();

        INSTANCE.get_or_init(|| Self::new().unwrap());

        INSTANCE.get().unwrap() as *const _ as *mut _
    }

    fn init(
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
    ) -> Result<(), String> {
        unsafe {
            let pool = Pool::instance();
            if (*pool).refcount == 0 {
                (*pool).re_init(chunk_size, chunk_count, fq_size, cq_size)?;
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

unsafe impl Send for Pool {}
unsafe impl Sync for Pool {}

impl Nic {
    /// # Description
    /// Attaching `pv::Nic` to network interface
    /// # Arguments
    /// `if_name` - network interface name \
    /// `chunk_size` - total size of packet payload \
    /// `chunk_count` - total count of chunk \
    /// `fq_size` - filling ring size \
    /// `cq_size` - completion ring size \
    /// `tx_size` - tx ring size \
    /// `rx_size` - rx ring size \
    /// `config_value` - initial value for config_map (default: kernel-only if None) \
    /// # Returns
    /// On success, returns `pv::Nic` bound to the network interface. \
    /// On failure, returns an error string.
    pub fn new(
        if_name: &str,
        chunk_size: usize,
        chunk_count: usize,
        fq_size: usize,
        cq_size: usize,
        tx_size: usize,
        rx_size: usize,
        config_value: Option<AfXdpRxConfig>,
    ) -> Result<Nic, String> {
        // Load and attach XDP program
        // BPF_OBJECT_PATH is set at compile time by build.rs
        let bpf_obj_path = env!("BPF_OBJECT_PATH");

        let bpf_obj_cstr =
            CString::new(bpf_obj_path).map_err(|e| format!("Failed to create CString: {}", e))?;

        // Program name in the BPF object file
        let prog_name_cstr = CString::new("xsk_packetvisor_prog")
            .map_err(|e| format!("Failed to create CString for program name: {}", e))?;

        // Initialize xdp_program_opts
        let mut opts = xdp_program_opts {
            sz: std::mem::size_of::<xdp_program_opts>(),
            obj: std::ptr::null_mut(),
            opts: std::ptr::null_mut(),
            prog_name: prog_name_cstr.as_ptr(),
            find_filename: std::ptr::null(),
            open_filename: bpf_obj_cstr.as_ptr(),
            pin_path: std::ptr::null(),
            id: 0,
            fd: 0,
        };

        // Create XDP program
        let prog = unsafe { xdp_program__create(&mut opts as *mut xdp_program_opts) };
        if prog.is_null() {
            return Err("Failed to create XDP program".to_string());
        }

        // Check for errors
        let err = unsafe { libxdp_get_error(prog as *const c_void) };
        if err != 0 {
            let mut errmsg = vec![0u8; 1024];
            unsafe {
                libxdp_strerror(
                    err.try_into().unwrap(),
                    errmsg.as_mut_ptr() as *mut c_char,
                    errmsg.len(),
                );
            }
            let err_str = unsafe {
                CStr::from_ptr(errmsg.as_ptr() as *const c_char)
                    .to_string_lossy()
                    .to_string()
            };
            return Err(format!("Failed to load XDP program: {} ({})", err_str, err));
        }

        // Find interface first to get ifindex
        let interface = interfaces()
            .into_iter()
            .find(|elem| elem.name.as_str() == if_name)
            .ok_or(format!("Interface {} not found.", if_name))?;

        // Get interface index
        let ifindex = interface.index as i32;

        // Attach XDP program to interface (try native mode first, fallback to SKB mode)
        let mut attach_mode = xdp_attach_mode_XDP_MODE_NATIVE;
        let mut ret = unsafe { xdp_program__attach(prog, ifindex, attach_mode, 0) };

        if ret != 0 {
            // Try SKB mode if native mode fails
            attach_mode = xdp_attach_mode_XDP_MODE_SKB;
            ret = unsafe { xdp_program__attach(prog, ifindex, attach_mode, 0) };
            if ret != 0 {
                let mut errmsg = vec![0u8; 1024];
                unsafe {
                    libxdp_strerror(ret, errmsg.as_mut_ptr() as *mut c_char, errmsg.len());
                }
                let err_str = unsafe {
                    CStr::from_ptr(errmsg.as_ptr() as *const c_char)
                        .to_string_lossy()
                        .to_string()
                };
                return Err(format!(
                    "Failed to attach XDP program to interface: {} ({})",
                    err_str, ret
                ));
            }
        }

        // Get xsks_map file descriptor for later use
        let bpf_obj = unsafe { xdp_program__bpf_obj(prog) };
        if bpf_obj.is_null() {
            return Err("Failed to get BPF object from XDP program".to_string());
        }

        let xsks_map_name =
            CString::new("xsks_map").map_err(|e| format!("Failed to create CString: {}", e))?;
        let xsks_map = unsafe { bpf_object__find_map_by_name(bpf_obj, xsks_map_name.as_ptr()) };
        if xsks_map.is_null() {
            return Err("Failed to find xsks_map in BPF object".to_string());
        }

        let xsks_map_fd = unsafe { bpf_map__fd(xsks_map) };
        if xsks_map_fd < 0 {
            return Err(format!(
                "Failed to get xsks_map file descriptor: {}",
                xsks_map_fd
            ));
        }

        // Get config_map file descriptor for passing values to XDP program
        let config_map_name =
            CString::new("config_map").map_err(|e| format!("Failed to create CString: {}", e))?;
        let config_map = unsafe { bpf_object__find_map_by_name(bpf_obj, config_map_name.as_ptr()) };
        let config_map_fd = if config_map.is_null() {
            -1 // Map not found, optional map
        } else {
            let fd = unsafe { bpf_map__fd(config_map) };
            if fd < 0 {
                -1 // Failed to get fd, optional map
            } else {
                fd
            }
        };

        /*****************************************************************
         * TODO: Update config_value to specific protocol configuration.
         *****************************************************************/

        // Update config_map immediately after XDP program is attached
        // This ensures the map is initialized before any packets are processed
        // Use provided config_value or default to kernel-only if None
        let init_config_value = config_value.unwrap_or_else(AfXdpRxConfig::kernel_only);
        if config_map_fd >= 0 {
            let key: i32 = 0;
            let key_ptr = &key as *const i32;
            let value_ptr = &init_config_value as *const AfXdpRxConfig;
            let update_ret = unsafe {
                bpf_map_update_elem(
                    config_map_fd,
                    key_ptr as *const c_void,
                    value_ptr as *const c_void,
                    BPF_ANY as u64,
                )
            };
            if update_ret != 0 {
                eprintln!(
                    "Warning: Failed to initialize config_map: ret={}",
                    update_ret
                );
            } else {
                // Verify the update
                let mut verify_value = AfXdpRxConfig::default();
                let verify_ret = unsafe {
                    bpf_map_lookup_elem(
                        config_map_fd,
                        key_ptr as *const c_void,
                        &mut verify_value as *mut AfXdpRxConfig as *mut c_void,
                    )
                };
                if verify_ret == 0 {
                    eprintln!(
                        "Debug: config_map initialized with value: {:?}",
                        verify_value
                    );
                } else {
                    eprintln!("Warning: Failed to verify config_map initialization");
                }
            }
        }

        // Store prog and attach_mode for cleanup later
        let xdp_prog = prog;
        let xdp_attach_mode = attach_mode;

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
                xdp_prog,
                xdp_attach_mode,
                ifindex,
                xsks_map_fd,
                config_map_fd,
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
                    let pool = Pool::instance();
                    (*pool).refcount += 1;
                }
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
            __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 {
                libxdp_flags: XSK_LIBXDP_FLAGS__INHIBIT_PROG_LOAD,
            },
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
                (*Pool::instance()).umem,
                &mut self.rxq,
                &mut self.txq,
                &mut self.umem_fq,
                &mut self.umem_cq,
                &xsk_cfg,
            )
        };

        if ret == 0 {
            let update_ret = unsafe { xsk_socket__update_xskmap(self.xsk, self.xsks_map_fd) };
            if update_ret != 0 {
                let msg = unsafe {
                    CStr::from_ptr(strerror(-update_ret))
                        .to_string_lossy()
                        .into_owned()
                };
                let message = format!("Error: {}", msg);
                return Err(format!("xsk_socket__update_xskmap failed: {}", message));
            }
        }

        if ret != 0 {
            match unsafe {
                let pool = Pool::instance();
                (*pool).refcount
            } {
                0 => {
                    // Pool Full-Fallback
                    unsafe { xsk_umem__delete((*Pool::instance()).umem) };
                    thread::sleep(Duration::from_millis(100));

                    Pool::init(chunk_size, chunk_count, fq_size, cq_size)?;
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
                let pool = Pool::instance();
                xsk_socket__create_shared(
                    &mut self.xsk,
                    if_ptr,
                    0,
                    (*pool).umem,
                    &mut self.rxq,
                    &mut self.txq,
                    &mut (*pool).umem_fq,
                    &mut (*pool).umem_cq,
                    &xsk_cfg,
                )
            };

            // Attach AF_XDP socket to xsks_map in XDP program
            if ret == 0 {
                let update_ret = unsafe { xsk_socket__update_xskmap(self.xsk, self.xsks_map_fd) };
                if update_ret != 0 {
                    let msg = unsafe {
                        CStr::from_ptr(strerror(-update_ret))
                            .to_string_lossy()
                            .into_owned()
                    };
                    let message = format!("Error: {}", msg);
                    return Err(format!("xsk_socket__update_xskmap failed: {}", message));
                }
            }

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
        if unsafe {
            let pool = Pool::instance();
            (*pool).refcount == 0
        } {
            let pool = Pool::instance();
            unsafe {
                self.umem_fq = (*pool).umem_fq;
                self.umem_cq = (*pool).umem_cq;

                let fq_ptr = alloc_zeroed_layout::<xsk_ring_prod>()?;
                let cq_ptr = alloc_zeroed_layout::<xsk_ring_cons>()?;
                (*pool).umem_fq = std::ptr::read(fq_ptr.cast::<xsk_ring_prod>());
                (*pool).umem_cq = std::ptr::read(cq_ptr.cast::<xsk_ring_cons>());
            };
        }

        unsafe {
            let pool = Pool::instance();
            let fq_size = (*pool).buffer_pool.borrow().fq_size;
            (*pool)
                .buffer_pool
                .borrow_mut()
                .reserve_fq(&mut self.umem_fq, fq_size)?;
        }

        Ok(())
    }

    /// # Description
    /// Allocate packet using Pool
    /// # Returns
    /// On success, returns `pv::Packet` with empty payload. \
    /// On failure, returns `None`.
    pub fn alloc_packet(&self) -> Option<Packet> {
        unsafe { (*Pool::instance()).try_alloc_packet() }
    }

    /// # Description
    /// Send packets \
    /// **\*Sent packets are removed from the vector.**
    /// # Arguments
    /// `packets` - Packets to send
    /// # Returns
    /// Number of packets sent
    pub fn send(&mut self, packets: &mut Vec<Packet>) -> usize {
        let pool = Pool::instance();
        let sent_count = unsafe {
            (*pool).buffer_pool.borrow_mut().send(
                packets,
                &self.xsk,
                &mut self.txq,
                &mut self.umem_cq,
            )
        };
        packets.drain(0..sent_count);

        sent_count
    }

    /// # Description
    /// Receive packets
    /// # Arguments
    /// `len` - Number of packets to receive
    /// # Returns
    /// Received packets
    pub fn receive(&mut self, len: usize) -> Vec<Packet> {
        let pool = Pool::instance();
        unsafe {
            (*(*pool).buffer_pool).borrow_mut().recv(
                &(*pool).buffer_pool,
                len,
                &self.xsk,
                &mut self.rxq,
                &mut self.umem_fq,
            )
        }
    }

    /// # Description
    /// Update configuration value in XDP program's config_map
    /// This allows passing values from user space to the XDP program
    /// # Arguments
    /// `key` - Map key (typically 0 for single-value config maps)
    /// `value` - Value to set in the map
    /// # Returns
    /// On success, returns `Ok(())`. On failure, returns an error string.
    pub fn update_config(&self, key: i32, value: AfXdpRxConfig) -> Result<(), String> {
        if self.config_map_fd < 0 {
            return Err("config_map not available".to_string());
        }

        let key_ptr = &key as *const i32;
        let value_ptr = &value as *const AfXdpRxConfig;

        let ret = unsafe {
            bpf_map_update_elem(
                self.config_map_fd,
                key_ptr as *const c_void,
                value_ptr as *const c_void,
                BPF_ANY as u64,
            )
        };

        if ret != 0 {
            let errno = if ret < 0 {
                -ret as i32
            } else {
                unsafe { *libc::__errno_location() }
            };
            let msg = unsafe {
                CStr::from_ptr(strerror(errno))
                    .to_string_lossy()
                    .into_owned()
            };
            return Err(format!(
                "Failed to update config_map: {} (ret={}, errno={})",
                msg, ret, errno
            ));
        }

        Ok(())
    }

    /// # Description
    /// Lookup configuration value from XDP program's config_map
    /// # Arguments
    /// `key` - Map key (typically 0 for single-value config maps)
    /// # Returns
    /// On success, returns the value. On failure, returns an error string.
    pub fn lookup_config(&self, key: i32) -> Result<AfXdpRxConfig, String> {
        if self.config_map_fd < 0 {
            return Err("config_map not available".to_string());
        }

        let mut value = AfXdpRxConfig::default();
        let ret = unsafe {
            bpf_map_lookup_elem(
                self.config_map_fd,
                &key as *const i32 as *const c_void,
                &mut value as *mut AfXdpRxConfig as *mut c_void,
            )
        };

        if ret != 0 {
            let msg = unsafe {
                CStr::from_ptr(strerror(-ret))
                    .to_string_lossy()
                    .into_owned()
            };
            return Err(format!("Failed to lookup config_map: {} ({})", msg, ret));
        }

        Ok(value)
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

    /// # Description
    /// Replace payload with new data. \
    /// Data can be memmoved if needed.
    /// # Arguments
    /// `new_data` - new packet payload
    /// # Returns
    /// On success, returns `None` and payload of `pv::Packet` is replaced with `new_data`. \
    /// On failure, returns an error string.
    pub fn replace_data(&mut self, new_data: &[u8]) -> Result<(), String> {
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

    /// # Description
    /// Get mutable payload
    pub fn get_buffer_mut(&mut self) -> &mut [u8] {
        unsafe {
            std::slice::from_raw_parts_mut(
                self.buffer.offset(self.start.try_into().unwrap()),
                self.end - self.start,
            )
        }
    }

    /// # Description
    /// Resize payload size to `new_size`
    /// # Arguments
    /// `new_size` - new packet payload size
    /// # Returns
    /// On success, return None and payload size of `pv::Packet` is replaced with `new_size`. \
    /// On failure, returns an error string.
    pub fn resize(&mut self, new_size: usize) -> Result<(), String> {
        if new_size > self.buffer_size {
            return Err(format!(
                "The requested size is to large. (Max = {})",
                self.buffer_size
            ));
        }

        let temp_end = self.end;

        if new_size > self.buffer_size - self.start {
            // Need to move data
            unsafe {
                copy(
                    self.buffer.offset(self.start.try_into().unwrap()),
                    self.buffer,
                    temp_end,
                );
            }
            self.start = 0;
            self.end = new_size;
            return Ok(());
        }

        self.end = self.start + new_size;
        Ok(())
    }

    /// # Description
    /// Dump packet payload as hex. Debugging purpose
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
                } else if count.is_multiple_of(8) {
                    print!(" ");
                    if count.is_multiple_of(16) {
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
        unsafe {
            // xsk_socket__delete automatically removes the socket from xsks_map
            // and handles cleanup, so we don't need to manually delete it

            // Detach XDP program from interface
            if !self.xdp_prog.is_null() {
                let _ = xdp_program__detach(self.xdp_prog, self.ifindex, self.xdp_attach_mode, 0);
                xdp_program__close(self.xdp_prog);
            }

            // xsk delete (this also removes from xsks_map internally)
            xsk_socket__delete(self.xsk);
            let pool = Pool::instance();
            (*pool).refcount -= 1;
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
