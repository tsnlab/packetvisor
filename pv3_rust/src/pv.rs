#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(clippy::missing_safety_doc)]
#![allow(clippy::useless_transmute)]
#![allow(dead_code)]

include!("bindings.rs");

use core::ffi::*;
use std::ffi::{CString};
use std::alloc::{alloc_zeroed, Layout};
use pnet::datalink::{interfaces, NetworkInterface};

const DEFAULT_HEADROOM: u32 = 256;

fn packet_dump(packet: &PvPacket) {
    let chunk_address = packet.private as u64;
    let buffer_address: *const u8 = packet.buffer.cast_const();

    let length:u32 = packet.size;
    let mut count: u32 = 0;

    unsafe {
        println!("---packet dump--- chunk addr: {}", chunk_address);

        loop {
            let read_offset: usize = count as usize;
            let read_address: *const u8 = buffer_address.add(read_offset);
            print!("{:02X?} ", std::ptr::read(read_address));

            count += 1;
            if count == length {
                break;
            } else if count % 8 == 0 {
                print!(" ");
                if count % 16 == 0 {
                    println!("");
                }
            }
        }
    }
    println!("\n-------\n");
}

#[derive(Debug)]
pub struct PvPacket {
    pub start: u32,         // payload offset pointing the start of payload. ex. 256
    pub end: u32,           // payload offset point the end of payload. ex. 1000
    pub size: u32,          // total size of buffer. ex. 2048
    pub buffer: *mut u8,    // buffer address.
    private: *mut c_void,       // DO NOT touch this.
}

impl PvPacket {
    fn new() -> PvPacket {
        PvPacket {
            start: 0,
            end: 0,
            size: 0,
            buffer: std::ptr::null_mut(),
            private: std::ptr::null_mut()
        }
    }
}

#[derive(Debug)]
pub struct PvNic {
    pub if_name: String,

    xsk: *mut xsk_socket,

    umem: *mut xsk_umem,
    buffer: *mut c_void,
    chunk_size: u32,

    chunk_pool: Vec<u64>,
    chunk_pool_idx: i32,

    /* XSK rings */
    fq: xsk_ring_prod,
    rx: xsk_ring_cons,
    cq: xsk_ring_cons,
    tx: xsk_ring_prod,
}

impl PvNic {
    fn new(if_name: &String, chunk_size: u32, chunk_count: i32) -> Option<PvNic> {
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

        if is_creation_failed == true {
            None
        } else {
            let a:PvNic = unsafe {
                PvNic {
                    if_name: if_name.clone(),
                    xsk: xsk_ptr.cast::<xsk_socket>(),
                    umem: umem_ptr.cast::<xsk_umem>(),   // umem is needed to be dealloc after using packetvisor library.
                    buffer: std::ptr::null_mut(),
                    chunk_size: chunk_size,
                    chunk_pool: Vec::with_capacity(chunk_count as usize),
                    chunk_pool_idx: chunk_count,
                    fq: std::ptr::read(fq_ptr.cast::<xsk_ring_prod>()),
                    rx: std::ptr::read(rx_ptr.cast::<xsk_ring_cons>()),
                    cq: std::ptr::read(cq_ptr.cast::<xsk_ring_cons>()),
                    tx: std::ptr::read(tx_ptr.cast::<xsk_ring_prod>()),
                    }
                };
            Some(a)
        }
    }
}

fn pv_alloc_(nic: &mut PvNic) -> u64 {
    if nic.chunk_pool_idx > 0 {
        nic.chunk_pool_idx -= 1;
        nic.chunk_pool[nic.chunk_pool_idx as usize]
    } else {
        return u64::MAX;
    }
}

pub fn pv_alloc(nic: &mut PvNic) ->  Option<PvPacket> {
    let idx: u64 = pv_alloc_(nic);

    match idx {
        u64::MAX => { None },
        _ => {
            let mut packet: PvPacket = PvPacket::new();
            packet.start = DEFAULT_HEADROOM;
            packet.end = DEFAULT_HEADROOM;
            packet.size = nic.chunk_size;
            packet.buffer = unsafe { xsk_umem__get_data(nic.buffer, idx).cast::<u8>() };
            packet.private = idx as *mut c_void;

            Some(packet)
        }
    }
}

fn pv_free_(nic: &mut PvNic, chunk_addr: u64) {
    // align **chunk_addr with chunk size
    let remainder = chunk_addr % (nic.chunk_size as u64);
    let chunk_addr = chunk_addr - remainder;
    nic.chunk_pool[nic.chunk_pool_idx as usize] = chunk_addr;
    nic.chunk_pool_idx += 1;
}

pub fn pv_free(nic: &mut PvNic, packets: &mut Vec<PvPacket>, index: usize) {
    pv_free_(nic, packets[index].private as u64);
    packets.remove(index);
}

fn configure_umem(nic: &mut PvNic, chunk_size: u32, chunk_count: u32, fill_size: u32, complete_size: u32) -> Result<(), i32> {
    let umem_buffer_size: usize = (chunk_count * chunk_size) as usize; // chunk_count is UMEM size.

    /* Reserve memory for the UMEM. */
    let mmap_address = unsafe {
        libc::mmap(std::ptr::null_mut::<libc::c_void>(), umem_buffer_size,
                    libc::PROT_READ | libc:: PROT_WRITE,
                libc::MAP_PRIVATE | libc::MAP_ANONYMOUS, -1, 0)
    };

    if mmap_address == libc::MAP_FAILED {
        return Err(-1);
    }

    /* create UMEM, filling ring, completion ring for xsk */
    let umem_cfg: xsk_umem_config = xsk_umem_config {
        // ring sizes aren't usually over 1024
        fill_size: fill_size,
        comp_size: complete_size,
        frame_size: chunk_size,
        frame_headroom: XSK_UMEM__DEFAULT_FRAME_HEADROOM,
        flags: XSK_UMEM__DEFAULT_FLAGS,
    };

    let ret:c_int = unsafe { xsk_umem__create(&mut nic.umem, mmap_address, umem_buffer_size as u64, &mut nic.fq, &mut nic.cq, &umem_cfg) };
    match ret {
        0 => {
            nic.buffer = mmap_address;
            Ok(())
        }
        _ => {
            unsafe { libc::munmap(mmap_address, umem_buffer_size); }
            Err(ret)
        }
    }
}

pub fn pv_open(if_name: &String, chunk_size: u32, chunk_count: u32,
                rx_ring_size: u32, tx_ring_size: u32, filling_ring_size: u32,
                completion_ring_size: u32) -> Option<PvNic>
{
    /* create PvNic */
    let nic_result: Option<PvNic> = PvNic::new(if_name, chunk_size, chunk_count as i32);   // **chunk_count is UMEM size
    let mut nic: PvNic;

    match nic_result {
        Some(T) => { nic = T; },
        None => { return None; }
    }

    /* initialize UMEM chunk information */
    for i in 0..chunk_count {
        nic.chunk_pool.push((i * chunk_size) as u64);   // put chunk address
    }

    /* configure UMEM */
    let ret = configure_umem(&mut nic, chunk_size, chunk_count, filling_ring_size, completion_ring_size);
    if ret.is_err() {
        return None;
    }

    /* pre-allocate UMEM chunks into fq */
    let mut fq_idx: u32 = 0;
    let reserved: u32 = unsafe { xsk_ring_prod__reserve(&mut nic.fq, filling_ring_size, &mut fq_idx) };

    for i in 0..reserved {
        unsafe { *xsk_ring_prod__fill_addr(&mut nic.fq, fq_idx + i) = pv_alloc_(&mut nic); } // allocation of UMEM chunks into fq.
    }
    unsafe { xsk_ring_prod__submit(&mut nic.fq, reserved); } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /* check interface name to be attached XDP program is valid */
    let all_interfaces: Vec<NetworkInterface> = interfaces();
    let is_exist: Option<&NetworkInterface> = all_interfaces
                            .iter()
                            .find(|element| element.name.as_str() == if_name.as_str());

    if is_exist.is_none() {
        return None;
    }

    let if_name = CString::new(if_name.as_str()).unwrap();
    let if_name_ptr: *const c_char = if_name.as_ptr() as *const c_char;

    /* setting xsk, RX ring, TX ring configuration */
    let xsk_cfg: xsk_socket_config = xsk_socket_config {
        rx_size: rx_ring_size,
        tx_size: tx_ring_size,
        /* zero means loading default XDP program.
        if you need to load other XDP program, set 1 on this flag and use xdp_program__open_file(), xdp_program__attach() in libxdp. */
        __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 { libxdp_flags: 0 },
        xdp_flags: 4,     // XDP_FLAGS_DRV_MODE in if_link.h (driver mode (Native mode))
        bind_flags: XDP_USE_NEED_WAKEUP as u16,
    };

    /* create xsk socket */
    let ret: c_int = unsafe { xsk_socket__create(&mut nic.xsk, if_name_ptr, 0, nic.umem, &mut nic.rx, &mut nic.tx, &xsk_cfg) };
    if ret != 0 {
        return None;
    }

    Some(nic)
}

// move ownership of nic
pub fn pv_close(nic: PvNic) -> c_int {
    /* xsk delete */
    unsafe { xsk_socket__delete(nic.xsk); }

    /* UMEM free */
    let ret: c_int = unsafe { xsk_umem__delete(nic.umem) };

    ret
}

pub fn pv_receive(nic: &mut PvNic, packets: &mut Vec<PvPacket>, batch_size: u32) -> u32 {
    /* pre-allocate UMEM chunks into fq as much as **batch_size to receive packets */
    let mut fq_idx: u32 = 0;
    let reserved: u32 = unsafe { xsk_ring_prod__reserve(&mut nic.fq, batch_size, &mut fq_idx)} ; // reserve slots in fq as much as **batch_size.

    for i in 0..reserved {
        unsafe { *xsk_ring_prod__fill_addr(&mut nic.fq, fq_idx + i) = pv_alloc_(nic); }   // allocate UMEM chunks into fq.
    }
    unsafe { xsk_ring_prod__submit(&mut nic.fq, reserved); } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /* save packet metadata from received packets in RX ring. */
    let mut rx_idx: u32 = 0;
    let mut received: u32 = unsafe { xsk_ring_cons__peek(&mut nic.rx, batch_size, &mut rx_idx) }; // fetch the number of received packets in RX ring.

    if received > 0 {
        let mut metadata_count: u32 = 0;
        while metadata_count < received {
            /* create packet metadata */
            packets.push(PvPacket::new());
            let rx_desc_ptr: *const xdp_desc = unsafe { xsk_ring_cons__rx_desc(&mut nic.rx, rx_idx + metadata_count) }; // bringing information(packet address, packet length) of received packets through descriptors in RX ring

            /* save metadata */
            packets[metadata_count as usize].start = DEFAULT_HEADROOM;
            packets[metadata_count as usize].end = DEFAULT_HEADROOM + unsafe { rx_desc_ptr.as_ref().unwrap().len };
            packets[metadata_count as usize].size = nic.chunk_size;
            packets[metadata_count as usize].buffer = unsafe { xsk_umem__get_data(nic.buffer, rx_desc_ptr.as_ref().unwrap().addr).cast::<u8>().sub(DEFAULT_HEADROOM as usize) };
            packets[metadata_count as usize].private = unsafe { (rx_desc_ptr.as_ref().unwrap().addr - DEFAULT_HEADROOM as u64) as *mut c_void };

            // packet_dump(&packets[metadata_count as usize]);
            metadata_count += 1;
        }

        unsafe { xsk_ring_cons__release(&mut nic.rx, received); } // notify kernel of using filled slots in RX ring as much as **received

        if metadata_count != received { received = metadata_count; }
    }

    unsafe {
        if xsk_ring_prod__needs_wakeup(&mut nic.fq) != 0 {
            libc::recvfrom(xsk_socket__fd(nic.xsk), std::ptr::null_mut::<libc::c_void>(), 0, libc::MSG_DONTWAIT, std::ptr::null_mut::<libc::sockaddr>(), std::ptr::null_mut::<u32>()) ;
        }
    }

    received
}

pub fn pv_send(nic: &mut PvNic, packets: &mut Vec<PvPacket>, batch_size: u32) -> u32 {
    /* free packet metadata and UMEM chunks as much as the number of filled slots in cq. */
    let mut cq_idx: u32 = 0;
    let filled: u32 = unsafe{ xsk_ring_cons__peek(&mut nic.cq, batch_size, &mut cq_idx) }; // fetch the number of filled slots(the number of packets completely sent) in cq

    if filled > 0 {
        for _ in 0..filled {
            unsafe { pv_free_(nic, *xsk_ring_cons__comp_addr(&nic.cq, cq_idx)); } // free UMEM chunks as much as the number of sent packets (same as **filled)
            cq_idx += 1;
        }
        unsafe { xsk_ring_cons__release(&mut nic.cq, filled); } // notify kernel that cq has empty slots with **filled (Dequeue)
    }

    /* reserve TX ring as much as batch_size before sending packets. */
    let mut tx_idx: u32 = 0;
    let reserved: u32 = unsafe { xsk_ring_prod__reserve(&mut nic.tx, batch_size, &mut tx_idx) };

    /* If reservation of slots in TX ring is not sufficient, Don't send them and return 0. */
    if reserved < batch_size {
        /* in case that kernel lost interrupt signal in the previous sending packets procedure,
        repeat to interrupt kernel to send packets which ketnel could have still held.
        (this procedure is for clearing filled slots in cq, so that cq can be reserved as much as **batch_size in the next execution of pv_send(). */
        unsafe{
            if xsk_ring_prod__needs_wakeup(&mut nic.tx) != 0 {
                libc::sendto(
                    xsk_socket__fd(nic.xsk),
                    std::ptr::null::<libc::c_void>(),
                    0 as libc::size_t,
                    libc::MSG_DONTWAIT,
                    std::ptr::null::<libc::sockaddr>(),
                    0 as libc::socklen_t);
            }
        }
        return 0;
    }

    /* send packets if reservation of slots in TX ring is same as batch_size */
    for i in 0..reserved {
        let pkt_index: usize = (reserved - 1 - i) as usize;
        /* insert packets to be send into TX ring (Enqueue) */
        let rx_desc_ptr = unsafe { xsk_ring_prod__tx_desc(&mut nic.tx, tx_idx + i) } ;
        unsafe {
            rx_desc_ptr.as_mut().unwrap().addr = packets[pkt_index].private as u64 + packets[pkt_index].start as u64;
            rx_desc_ptr.as_mut().unwrap().len = packets[pkt_index].end - packets[pkt_index].start;
        }
        // packet_dump(&packets[pkt_index]);
        packets.pop();   // free packet metadata of sent packets.
    }

    unsafe {
        xsk_ring_prod__submit(&mut nic.tx, reserved);  // notify kernel of enqueuing TX ring as much as reserved.

        if xsk_ring_prod__needs_wakeup(&mut nic.tx) != 0 {
            libc::sendto(xsk_socket__fd(nic.xsk), std::ptr::null::<libc::c_void>(), 0 as libc::size_t, libc::MSG_DONTWAIT, std::ptr::null::<libc::sockaddr>(), 0 as libc::socklen_t);   // interrupt kernel to send packets.
        }
    }

    reserved
}
