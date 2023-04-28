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

// TODO: 러스트로 포팅
// struct pv_packet* pv_alloc(struct pv_nic* nic) {
//     uint64_t idx = _pv_alloc(nic);
//     if (idx == INVALID_CHUNK_INDEX) {
//         return NULL;
//     }

//     struct pv_packet* packet = calloc(1, sizeof(struct pv_packet));
//     if (packet == NULL) {
//         return NULL;
//     }

//     packet->start = DEFAULT_HEADROOM;//테스트 필요
//     packet->end = DEFAULT_HEADROOM;
//     packet->size = nic->chunk_size;
//     packet->buffer = xsk_umem__get_data(nic->buffer, idx);
//     packet->priv = (void*)idx;

//     return packet;
// }


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
    xsk: *mut xsk_socket,

    umem: *mut xsk_umem,
    buffer: *mut c_void,
    chunk_size: u32,

    // chunk_pool: *mut Vec<u64>,
    chunk_pool: Vec<u64>,
    chunk_pool_idx: i32,

    /* XSK rings */
    fq: xsk_ring_prod,
    rx: xsk_ring_cons,
    cq: xsk_ring_cons,
    tx: xsk_ring_prod,
}

impl PvNic {
    // TODO: Change return type with Option<PvNic> for safety.
    fn new(chunk_size: u32, chunk_count: i32) -> PvNic {

        let xsk_ptr;
        unsafe {
            let xsk_layout = Layout::new::<xsk_socket>();
            xsk_ptr = alloc_zeroed(xsk_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let umem_ptr;
        unsafe {
            let umem_layout = Layout::new::<xsk_umem>();
            umem_ptr = alloc_zeroed(umem_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let fq_ptr;
        unsafe {
            let fq_layout = Layout::new::<xsk_ring_prod>();
            fq_ptr = alloc_zeroed(fq_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let rx_ptr;
        unsafe {
            let rx_layout = Layout::new::<xsk_ring_cons>();
            rx_ptr = alloc_zeroed(rx_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let cq_ptr;
        unsafe {
            let cq_layout = Layout::new::<xsk_ring_cons>();
            cq_ptr = alloc_zeroed(cq_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let tx_ptr;
        unsafe {
            let tx_layout = Layout::new::<xsk_ring_prod>();
            tx_ptr = alloc_zeroed(tx_layout);
            // TODO: Add if umem_ptr.is_null() to make it safe
        }

        let a:PvNic = unsafe {
            PvNic {
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

        a
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

fn _pv_free(nic: &mut PvNic, chunk_addr: u64) {
    // align **chunk_addr with chunk size
    let remainder = chunk_addr % (nic.chunk_size as u64);
    let chunk_addr = chunk_addr - remainder;
    nic.chunk_pool[nic.chunk_pool_idx as usize] = chunk_addr;
    nic.chunk_pool_idx += 1;
}

/* TODO: packet_process() 구현하면서 remove를 사용해서 벡터중 해당 요소만 제거하는 것으로 소스짜기 (WIP)*/
pub fn pv_free(nic: &mut PvNic, packets: &mut Vec<PvPacket>) {
    let packet_count: usize = packets.len();

    for i in 0..packet_count {
        _pv_free(nic, packets[packet_count - 1 - i].private as u64);
    }
    packets.clear();
    // packets.remove(1); // WIP

}

fn configure_umem(nic: &mut PvNic, chunk_size: u32, chunk_count: u32, fill_size: u32, complete_size: u32) -> Result<(), i32> {
    let umem_buffer_size: usize = (chunk_count * chunk_size) as usize;
    let mmap_address = unsafe {
        libc::mmap(std::ptr::null_mut(), umem_buffer_size,
                    libc::PROT_READ | libc:: PROT_WRITE,
                libc::MAP_PRIVATE | libc::MAP_ANONYMOUS, -1, 0)
    };

    let umem_cfg: xsk_umem_config = xsk_umem_config {
        fill_size: fill_size,
        comp_size: complete_size,
        frame_size: chunk_size,
        frame_headroom: XSK_UMEM__DEFAULT_FRAME_HEADROOM,
        flags: XSK_UMEM__DEFAULT_FLAGS,
    };

    let ret:c_int;

    unsafe {
        ret = xsk_umem__create(&mut nic.umem, mmap_address, umem_buffer_size as u64, &mut nic.fq, &mut nic.cq, &umem_cfg);
    }

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

pub fn pv_open(if_name: String, chunk_size: u32, chunk_count: u32,
                rx_ring_size: u32, tx_ring_size: u32, filling_ring_size: u32,
                completion_ring_size: u32) -> Option<PvNic>
{
    /* create PvNic */
    let mut nic: PvNic = PvNic::new(chunk_size, chunk_count as i32);

    /* initialize UMEM chunk information */
    for i in 0..chunk_count {
        nic.chunk_pool.push((i * chunk_size) as u64);   // put chunk address
    }

    /* configure UMEM */
    let ret = configure_umem(&mut nic, chunk_size, chunk_count, filling_ring_size, completion_ring_size);
    if ret.is_err() {
        return None
    }

    /* pre-allocate UMEM chunks into fq */
    let mut fq_idx: u32 = 0;
    let reserved: u32 = unsafe { xsk_ring_prod__reserve(&mut nic.fq, filling_ring_size, &mut fq_idx) };

    for _ in 0..reserved {
        unsafe {
            let pv_alloc = pv_alloc_(&mut nic);
            *xsk_ring_prod__fill_addr(&mut nic.fq, fq_idx) = pv_alloc;
            fq_idx += 1;
        }
    }
    unsafe { xsk_ring_prod__submit(&mut nic.fq, reserved); }// notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /*setting xsk, RX ring, TX ring configuration */
    let if_name = CString::new(if_name.as_str()).unwrap();
    let if_name_ptr: *const c_char = if_name.as_ptr() as *const c_char;
    let ret = unsafe { libc::if_nametoindex(if_name_ptr) };

    if ret == 0 {
        return None
    }

    let xsk_cfg: xsk_socket_config = xsk_socket_config {
        rx_size: rx_ring_size,
        tx_size: tx_ring_size,
        /* zero means loading default XDP program.
        if you need to load other XDP program, set 1 on this flag and use xdp_program__open_file(), xdp_program__attach() in libxdp. */
        __bindgen_anon_1: xsk_socket_config__bindgen_ty_1 { libxdp_flags: 0 },
        xdp_flags: 4,     // XDP_FLAGS_DRV_MODE (driver mode (Native mode))
        bind_flags: XDP_USE_NEED_WAKEUP as u16,
    };

    let ret: c_int = unsafe { xsk_socket__create(&mut nic.xsk, if_name_ptr, 0, nic.umem, &mut nic.rx, &mut nic.tx, &xsk_cfg) };
    if ret != 0 {
        return None
    }

    Some(nic)
}

// move ownership of nic
// 정상 동작 확인
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

    for _ in 0.. reserved {
        unsafe { *xsk_ring_prod__fill_addr(&mut nic.fq, fq_idx) = pv_alloc_(nic); } // reserve slots in fq as much as **batch_size.
        fq_idx += 1;
    }
    unsafe { xsk_ring_prod__submit(&mut nic.fq, reserved); } // notify kernel of allocating UMEM chunks into fq as much as **reserved.

    /* save packet metadata from received packets in RX ring. */
    let mut rx_idx: u32 = 0;
    let mut received: u32 = unsafe { xsk_ring_cons__peek(&mut nic.rx, batch_size, &mut rx_idx) };

    if received > 0 {
        let mut metadata_count: u32 = 0;
        while metadata_count < received {
            packets.push(PvPacket::new());
            // println!("rx_idx: {}", rx_idx);
            let rx_desc_ptr = unsafe { xsk_ring_cons__rx_desc(&mut nic.rx, rx_idx + metadata_count) };
            /* save metadata */
            packets[metadata_count as usize].start = DEFAULT_HEADROOM;
            packets[metadata_count as usize].end = DEFAULT_HEADROOM + unsafe { rx_desc_ptr.as_ref().unwrap().len };
            packets[metadata_count as usize].size = nic.chunk_size;
            packets[metadata_count as usize].buffer = unsafe { xsk_umem__get_data(nic.buffer, rx_desc_ptr.as_ref().unwrap().addr).cast::<u8>().sub(DEFAULT_HEADROOM as usize) };
            packets[metadata_count as usize].private = unsafe { (rx_desc_ptr.as_ref().unwrap().addr - DEFAULT_HEADROOM as u64) as *mut c_void };
            // println!("packet: {:?}", packets[metadata_count as usize]);
            packet_dump(&packets[metadata_count as usize]);
            metadata_count += 1;
        }

        unsafe { xsk_ring_cons__release(&mut nic.rx, received); }

        if metadata_count != received {
            received = metadata_count;
        }
    }

    unsafe {
        if xsk_ring_prod__needs_wakeup(&mut nic.fq) != 0 {
            libc::recvfrom(xsk_socket__fd(nic.xsk), std::ptr::null_mut::<c_void>(), 0, libc::MSG_DONTWAIT, std::ptr::null_mut::<libc::sockaddr>(), std::ptr::null_mut::<u32>()) ;
        }
    }

    received
}

// TODO: 러스트로 포팅
// uint32_t pv_send(struct pv_nic* nic, struct pv_packet** packets, uint32_t batch_size) {
//     /* free packet metadata and UMEM chunks as much as the number of filled slots in cq. */
//     uint32_t cq_idx;
//     uint32_t filled = xsk_ring_cons__peek(&nic->cq, batch_size, &cq_idx); // fetch the number of filled slots(the number of packets completely sent) in cq

//     if (filled > 0) {
//         for (uint32_t i = 0; i < filled; i++) {
//             _pv_free(nic, *xsk_ring_cons__comp_addr(&nic->cq, cq_idx++));   // free UMEM chunks as much as the number of sent packets (same as **filled)
//         }
//         xsk_ring_cons__release(&nic->cq, filled); // notify kernel that cq has empty slots with **filled (Dequeue)
//     }

//     /* reserve TX ring as much as batch_size before sending packets. */
//     uint32_t tx_idx = 0;
//     uint32_t reserved = xsk_ring_prod__reserve(&nic->tx, batch_size, &tx_idx);
//     /* send packets if TX ring has been reserved with **batch_size. if not, don't send packets and free them */
//     if (reserved < batch_size) {
//         /* in case that kernel lost interrupt signal in the previous sending packets procedure,
//         repeat to interrupt kernel to send packets which ketnel could have still held.
//         (this procedure is for clearing filled slots in cq, so that cq can be reserved as much as **batch_size in the next execution of pv_send(). */
//         if (xsk_ring_prod__needs_wakeup(&nic->tx)) {
//             sendto(xsk_socket__fd(nic->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);
//         }
//         return 0;
//     }

//     /* send packets */
//     struct xdp_desc* tx_desc;
//     for (uint32_t i = 0; i < reserved; i++) {
//         /* insert packets to be send into TX ring (Enqueue) */
//         struct pv_packet* packet = packets[i];
//         tx_desc = xsk_ring_prod__tx_desc(&nic->tx, tx_idx + i);
//         tx_desc->addr = (uint64_t)(packet->priv + packet->start);
//         tx_desc->len = packet->end - packet->start;

//         #ifdef _DEBUG
//         printf("addr: %llu, len: %u\n", tx_desc->addr, tx_desc->len);
//         hex_dump(packet->buffer, packet->size, (uint64_t)packet->priv);
//         #endif
//         free(packet);   // free packet metadata of sent packets.
//     }

//     xsk_ring_prod__submit(&nic->tx, reserved);  // notify kernel of enqueuing TX ring as much as reserved.

//     if (xsk_ring_prod__needs_wakeup(&nic->tx)) {
//         sendto(xsk_socket__fd(nic->xsk), NULL, 0, MSG_DONTWAIT, NULL, 0);   // interrupt kernel to send packets.
//     }

//     return reserved;
// }
