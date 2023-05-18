use crate::pv::PvPacket;
use pnet::datalink::MacAddr;

pub fn endian16(ptr: *const u8) -> u16 {
    (unsafe { *(ptr.offset(1)) } as u16) << 8 | unsafe { *ptr } as u16
}

pub fn endian32(ptr: *const u8) -> u32 {
    unsafe {
        ((*ptr.offset(3) as u32) << 24)
            | ((*ptr.offset(2) as u32) << 16)
            | ((*ptr.offset(1) as u32) << 8)
            | ((*ptr.offset(0) as u32) << 0)
    }
}

pub fn endian48(ptr: *const u8) -> u64 {
    unsafe {
        ((*ptr.offset(5) as u64) << 40)
            | ((*ptr.offset(4) as u64) << 32)
            | ((*ptr.offset(3) as u64) << 24)
            | ((*ptr.offset(2) as u64) << 16)
            | ((*ptr.offset(1) as u64) << 8)
            | ((*ptr.offset(0) as u64) << 0)
    }
}

pub fn endian64(ptr: *const u8) -> u64 {
    unsafe {
        ((*ptr.offset(7) as u64) << 56)
            | ((*ptr.offset(6) as u64) << 48)
            | ((*ptr.offset(5) as u64) << 40)
            | ((*ptr.offset(4) as u64) << 32)
            | ((*ptr.offset(3) as u64) << 24)
            | ((*ptr.offset(2) as u64) << 16)
            | ((*ptr.offset(1) as u64) << 8)
            | ((*ptr.offset(0) as u64) << 0)
    }
}

pub fn macAddr_to_u64(mac: &MacAddr) -> u64 {
    let mut ret: u64 = 0;
    let mut n = 0;

    let mac = mac.octets();
    for i in mac.iter() {
        ret |= (*i as u64) << (n * 8);
        n += 1;
    }
    ret
}

fn set_bits(ptr: *mut u8, value: u64, len: u32) {
    let mut n = 0;
    unsafe {
        while len > n {
            *ptr.offset(n as isize) = (value >> (n * 8)) as u8;
            n += 1;
        }
    }
}

pub fn check(packet: &PvPacket, pos: u32, size: u32) -> bool {
    if packet.start + pos + size > packet.end {
        false
    } else {
        true
    }
}

pub fn get_hw_type(packet: &PvPacket) -> Result<u16, &str> {
    if check(packet, 14, 2) {
        Ok(endian16(unsafe { packet.buffer.offset((packet.start + 14) as isize) }))
    } else {
        Err("ARP: get_hw_type() failed")
    }
}

pub fn set_hw_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    if check(packet, 14, 2) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 14) as isize), v as u64, 2);
        };
        Ok(packet)
    } else {
        Err("ARP: set_hw_type() failed")
    }
}


pub fn get_ether_type(packet: &PvPacket) -> Result<u16, &str> {
    if check(packet, 12, 2) {
        Ok(endian16(unsafe { packet.buffer.offset((packet.start + 12) as isize) }))
    } else {
        Err("ARP: get_ether_type() failed")
    }
}

pub fn set_ether_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    if check(packet, 12, 2) && check(packet, 16, 2) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 12) as isize), v as u64, 2);
        };
        Ok(packet)
    } else {
        Err("ARP: set_ether_type() failed")
    }
}

pub fn get_proto_type(packet: &PvPacket) -> Result<u16, &str> {
    if check(packet, 16, 2) {
        Ok(endian16(unsafe { packet.buffer.offset((packet.start + 16) as isize) }))
    } else {
        Err("ARP: get_proto_type() failed")
    }
}

pub fn set_proto_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    if check(packet, 16, 2) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 16) as isize), v as u64, 2);
        };
        Ok(packet)
    } else {
        Err("ARP: set_proto_type() failed")
    }
}

pub fn get_hw_size(packet: &PvPacket) -> Result<u8, &str> {
    if check(packet, 18, 1) {
        Ok(unsafe { *packet.buffer.offset((packet.start + 18) as isize) })
    } else {
        Err("ARP: get_hw_size() failed")
    }
}

pub fn set_hw_size(packet: &mut PvPacket, v: u8) -> Result<&PvPacket, &str> {
    if check(packet, 18, 1) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 18) as isize), v as u64, 1);
        };
        Ok(packet)
    } else {
        Err("ARP: set_hw_size() failed")
    }
}

pub fn get_proto_size(packet: &PvPacket) -> Result<u8, &str> {
    if check(packet, 19, 1) {
        Ok(unsafe { *packet.buffer.offset((packet.start + 19) as isize) })
    } else {
        Err("ARP: get_proto_size() failed")
    }
}

pub fn set_proto_size(packet: &mut PvPacket, v: u8) -> Result<&PvPacket, &str> {
    if check(packet, 19, 1) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 19) as isize), v as u64, 1);
        };
        Ok(packet)
    } else {
        Err("ARP: set_proto_size() failed")
    }
}

pub fn get_op_code(packet: &PvPacket) -> Result<u16, &str> {
    if check(packet, 20, 2) {
        Ok(endian16(unsafe { packet.buffer.offset((packet.start + 20) as isize) }))
    } else {
        Err("ARP: get_op_code() failed")
    }
}

pub fn set_op_code(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    if check(packet, 20, 2) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 20) as isize), v as u64, 2);
        };
        Ok(packet)
    } else {
        Err("ARP: set_op_code() failed")
    }
}

pub fn get_src_hw(packet: &PvPacket) -> Result<u64, &str> {
    if check(packet, 6, 6) && check(packet, 22, 6) {
        Ok(endian48(unsafe { packet.buffer.offset((packet.start + 6) as isize) }))
    } else {
        Err("ARP: get_src_hw() failed")
    }
}

pub fn set_src_hw(packet: &mut PvPacket, v: u64) -> Result<&PvPacket, &str> {
    if check(packet, 6, 6) && check(packet, 22, 6) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 6) as isize), v as u64, 6);
            set_bits(packet.buffer.offset((packet.start + 22) as isize), v as u64, 6);
        };
        Ok(packet)
    } else {
        Err("ARP: set_src_hw() failed")
    }
}

pub fn get_src_proto(packet: &mut PvPacket) -> Result<u32, &str> {
    if check(packet, 28, 4) {
        return Ok(endian32(unsafe { packet.buffer.offset((packet.start + 28) as isize) }));
    } else {
        return Err("ARP: get_src_proto() failed");
    }
}

pub fn set_src_proto(packet: &mut PvPacket, v: u32) -> Result<&PvPacket, &str> {
    if check(packet, 28, 4) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 28) as isize), v as u64, 4);
        };
        Ok(packet)
    } else {
        Err("ARP: set_src_proto() failed")
    }
}

pub fn get_dst_hw(packet: &PvPacket) -> Result<u64, &str> {
    if check(packet, 0, 6) {
        return Ok(endian48(unsafe { packet.buffer.offset((packet.start) as isize) }));
    } else {
        return Err("ARP: get_dst_hw() failed");
    }
}

pub fn set_dst_hw(packet: &mut PvPacket, v: u64) -> Result<&PvPacket, &str> {
    if check(packet, 0, 6) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start) as isize), v as u64, 6);
            set_bits(packet.buffer.offset((packet.start + 32) as isize), v as u64, 6);
        };
        Ok(packet)
    } else {
        Err("ARP: set_dst_hw() failed")
    }
}

pub fn get_dst_proto(packet: &PvPacket) -> Result<u32, &str> {
    if check(packet, 38, 4) {
        return Ok(endian32(unsafe { packet.buffer.offset((packet.start + 38) as isize) }));
    } else {
        return Err("ARP: get_dst_proto() failed");
    }
}

pub fn set_dst_proto(packet: &mut PvPacket, v: u32) -> Result<&PvPacket, &str> {
    if check(packet, 38, 4) {
        unsafe {
            set_bits(packet.buffer.offset((packet.start + 38) as isize), v as u64, 4);
        };
        Ok(packet)
    } else {
        Err("ARP: set_dst_proto() failed")
    }
}

pub fn gen_arp_response_packet(
    packet: &mut PvPacket,
    src_mac_address: &MacAddr,
) {
    const ARP_RSPN_PACKET_SIZE: u32 = 42;

    // copy dest MAC
    set_dst_hw(packet, get_src_hw(packet).unwrap()).expect("ARP: gen_arp_response_packet() failed");
    // copy src MAC
    set_src_hw(packet, macAddr_to_u64(src_mac_address))
        .expect("ARP: gen_arp_response_packet() failed");
    // Ethertype - ARP
    set_ether_type(packet, endian16(&[0x08, 0x06][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // HW type
    set_hw_type(packet, endian16(&[0x00, 0x01][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // protocol type
    set_proto_type(packet, endian16(&[0x08, 0x00][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // // HW size
    set_hw_size(packet, 6).expect("ARP: gen_arp_response_packet() failed");
    // protocol size
    set_proto_size(packet, 4).expect("ARP: gen_arp_response_packet() failed");
    // op code - ARP_REPLY
    set_op_code(packet, endian16(&[0x00, 0x02][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // sender IP (10.0.0.4)
    set_src_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x04][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // dest IP (10.0.0.5)
    set_dst_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x05][0]))
        .expect("ARP: gen_arp_response_packet() failed");

    // set packet length of ARP response as 42 bytes
    packet.end = packet.start + ARP_RSPN_PACKET_SIZE;
}
