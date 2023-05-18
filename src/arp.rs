use crate::pv::PvPacket;
use crate::utils::*;
use pnet::datalink::MacAddr;

pub fn get_hw_type(packet: &PvPacket) -> Result<u16, &str> {
    match get(packet, 14, 2) {
        Ok(v) => Ok(v as u16),
        Err(_) => Err("ARP: get_hw_type() failed"),
    }
}

pub fn set_hw_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    match set(packet, 14, 2, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_hw_type() failed"),
    }
}

pub fn get_ether_type(packet: &PvPacket) -> Result<u16, &str> {
    let ret = get(packet, 12, 2);
    match ret {
        Ok(v) => Ok(v as u16),
        Err(_) => Err("ARP: get_ether_type() failed"),
    }
}

pub fn set_ether_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    match set(packet, 12, 2, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_ether_type() failed"),
    }
}

pub fn get_proto_type(packet: &PvPacket) -> Result<u16, &str> {
    match get(packet, 16, 2) {
        Ok(v) => Ok(v as u16),
        Err(_) => Err("ARP: get_proto_type() failed"),
    }
}

pub fn set_proto_type(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    match set(packet, 16, 2, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_proto_type() failed"),
    }
}

pub fn get_hw_size(packet: &PvPacket) -> Result<u8, &str> {
    match get(packet, 18, 1) {
        Ok(v) => Ok(v as u8),
        Err(_) => Err("ARP: get_hw_size() failed"),
    }
}

pub fn set_hw_size(packet: &mut PvPacket, v: u8) -> Result<&PvPacket, &str> {
    match set(packet, 18, 1, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_hw_size() failed"),
    }
}

pub fn get_proto_size(packet: &PvPacket) -> Result<u8, &str> {
    match get(packet, 19, 1) {
        Ok(v) => Ok(v as u8),
        Err(_) => Err("ARP: get_proto_size() failed"),
    }
}

pub fn set_proto_size(packet: &mut PvPacket, v: u8) -> Result<&PvPacket, &str> {
    match set(packet, 19, 1, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_proto_size() failed"),
    }
}

pub fn get_op_code(packet: &PvPacket) -> Result<u16, &str> {
    match get(packet, 20, 2) {
        Ok(v) => Ok(v as u16),
        Err(_) => Err("ARP: get_op_code() failed"),
    }
}

pub fn set_op_code(packet: &mut PvPacket, v: u16) -> Result<&PvPacket, &str> {
    match set(packet, 20, 2, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_op_code() failed"),
    }
}

pub fn get_src_hw(packet: &PvPacket) -> Result<u64, &str> {
    match get(packet, 6, 6) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: get_src_hw() failed"),
    }
}

pub fn set_src_hw(packet: &mut PvPacket, v: u64) -> Result<&PvPacket, &str> {
    match set(packet, 6, 6, v) {
        Ok(v) => {}
        Err(_) => {
            return Err("ARP: set_src_hw() failed");
        }
    }
    match set(packet, 22, 6, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_src_hw() failed"),
    }
}

pub fn get_src_proto(packet: &mut PvPacket) -> Result<u32, &str> {
    match get(packet, 28, 4) {
        Ok(v) => Ok(v as u32),
        Err(_) => Err("ARP: get_src_proto() failed"),
    }
}

pub fn set_src_proto(packet: &mut PvPacket, v: u32) -> Result<&PvPacket, &str> {
    match set(packet, 28, 4, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_src_proto() failed"),
    }
}

pub fn get_dst_hw(packet: &PvPacket) -> Result<u64, &str> {
    match get(packet, 0, 6) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: get_dst_hw() failed"),
    }
}

pub fn set_dst_hw(packet: &mut PvPacket, v: u64) -> Result<&PvPacket, &str> {
    match set(packet, 0, 6, v) {
        Ok(v) => {}
        Err(_) => {
            return Err("ARP: set_dst_hw() failed");
        }
    }
    match set(packet, 32, 6, v) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_dst_hw() failed"),
    }
}

pub fn get_dst_proto(packet: &PvPacket) -> Result<u32, &str> {
    match get(packet, 38, 4) {
        Ok(v) => Ok(v as u32),
        Err(_) => Err("ARP: get_dst_proto() failed"),
    }
}

pub fn set_dst_proto(packet: &mut PvPacket, v: u32) -> Result<&PvPacket, &str> {
    match set(packet, 38, 4, v as u64) {
        Ok(v) => Ok(v),
        Err(_) => Err("ARP: set_dst_proto() failed"),
    }
}

pub fn gen_arp_response_packet(packet: &mut PvPacket, src_mac_address: &MacAddr) {
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
    set_hw_type(packet, endian16(&[0x00, 0x01][0])).expect("ARP: gen_arp_response_packet() failed");
    // protocol type
    set_proto_type(packet, endian16(&[0x08, 0x00][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // // HW size
    set_hw_size(packet, 6).expect("ARP: gen_arp_response_packet() failed");
    // protocol size
    set_proto_size(packet, 4).expect("ARP: gen_arp_response_packet() failed");
    // op code - ARP_REPLY
    set_op_code(packet, endian16(&[0x00, 0x02][0])).expect("ARP: gen_arp_response_packet() failed");
    // sender IP (10.0.0.4)
    set_src_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x04][0]))
        .expect("ARP: gen_arp_response_packet() failed");
    // dest IP (10.0.0.5)
    set_dst_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x05][0]))
        .expect("ARP: gen_arp_response_packet() failed");

    // set packet length of ARP response as 42 bytes
    packet.end = packet.start + ARP_RSPN_PACKET_SIZE;
}
