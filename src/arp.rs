use crate::pv::PvPacket;
use crate::utils::*;
use pnet::datalink::MacAddr;

pub fn get_hw_type(packet: &PvPacket) -> Result<u16, u32> {
    match get(packet, 14, 2) {
        Ok(value) => Ok(value as u16),
        Err(e) => Err(e)
    }
}

pub fn set_hw_type(packet: &mut PvPacket, value: u16) -> Result<&PvPacket, u32> {
    match set(packet, 14, 2, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_ether_type(packet: &PvPacket) -> Result<u16, u32> {
    let ret = get(packet, 12, 2);
    match ret {
        Ok(value) => Ok(value as u16),
        Err(e) => Err(e)
    }
}

pub fn set_ether_type(packet: &mut PvPacket, value: u16) -> Result<&PvPacket, u32> {
    match set(packet, 12, 2, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_proto_type(packet: &PvPacket) -> Result<u16, u32> {
    match get(packet, 16, 2) {
        Ok(value) => Ok(value as u16),
        Err(e) => Err(e)
    }
}

pub fn set_proto_type(packet: &mut PvPacket, value: u16) -> Result<&PvPacket, u32> {
    match set(packet, 16, 2, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_hw_size(packet: &PvPacket) -> Result<u8, u32> {
    match get(packet, 18, 1) {
        Ok(value) => Ok(value as u8),
        Err(e) => Err(e)
    }
}

pub fn set_hw_size(packet: &mut PvPacket, value: u8) -> Result<&PvPacket, u32> {
    match set(packet, 18, 1, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_proto_size(packet: &PvPacket) -> Result<u8, u32> {
    match get(packet, 19, 1) {
        Ok(value) => Ok(value as u8),
        Err(e) => Err(e)
    }
}

pub fn set_proto_size(packet: &mut PvPacket, value: u8) -> Result<&PvPacket, u32> {
    match set(packet, 19, 1, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_op_code(packet: &PvPacket) -> Result<u16, u32> {
    match get(packet, 20, 2) {
        Ok(value) => Ok(value as u16),
        Err(e) => Err(e)
    }
}

pub fn set_op_code(packet: &mut PvPacket, value: u16) -> Result<&PvPacket, u32> {
    match set(packet, 20, 2, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_src_hw(packet: &PvPacket) -> Result<u64, u32> {
    match get(packet, 6, 6) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn set_src_hw(packet: &mut PvPacket, value: u64) -> Result<&PvPacket, u32> {
    match set(packet, 6, 6, value) {
        Ok(value) => {}
        Err(e) => {
            return Err(e);
        }
    }
    match set(packet, 22, 6, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_src_proto(packet: &mut PvPacket) -> Result<u32, u32> {
    match get(packet, 28, 4) {
        Ok(value) => Ok(value as u32),
        Err(e) => Err(e)
    }
}

pub fn set_src_proto(packet: &mut PvPacket, value: u32) -> Result<&PvPacket, u32> {
    match set(packet, 28, 4, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_dst_hw(packet: &PvPacket) -> Result<u64, u32> {
    match get(packet, 0, 6) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn set_dst_hw(packet: &mut PvPacket, value: u64) -> Result<&PvPacket, u32> {
    match set(packet, 0, 6, value) {
        Ok(_) => {},
        Err(e) => {
            return Err(e);
        }
    }
    match set(packet, 32, 6, value) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn get_dst_proto(packet: &PvPacket) -> Result<u32, u32> {
    match get(packet, 38, 4) {
        Ok(value) => Ok(value as u32),
        Err(e) => Err(e)
    }
}

pub fn set_dst_proto(packet: &mut PvPacket, value: u32) -> Result<&PvPacket, u32> {
    match set(packet, 38, 4, value as u64) {
        Ok(value) => Ok(value),
        Err(e) => Err(e)
    }
}

pub fn gen_arp_response_packet(packet: &mut PvPacket, src_mac_address: &MacAddr) -> Result<u32, u32> {
    const ARP_RSPN_PACKET_SIZE: u32 = 42;

    // copy dest MAC
    set_dst_hw(packet, get_src_hw(packet)?)?;
    // copy src MAC
    set_src_hw(packet, macAddr_to_u64(src_mac_address))?;
    // Ethertype - ARP
    set_ether_type(packet, endian16(&[0x08, 0x06][0]))?;
    // HW type
    set_hw_type(packet, endian16(&[0x00, 0x01][0]))?;
    // protocol type
    set_proto_type(packet, endian16(&[0x08, 0x00][0]))?;
    // // HW size
    set_hw_size(packet, 6)?;
    // protocol size
    set_proto_size(packet, 4)?;
    // op code - ARP_REPLY
    set_op_code(packet, endian16(&[0x00, 0x02][0]))?;
    // sender IP (10.0.0.4)
    set_src_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x04][0]))?;
    // dest IP (10.0.0.5)
    set_dst_proto(packet, endian32(&[0x0a, 0x00, 0x00, 0x05][0]))?;

    // set packet length of ARP response as 42 bytes
    packet.end = packet.start + ARP_RSPN_PACKET_SIZE;
    Ok(0)
}
