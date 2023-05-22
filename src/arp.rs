use std::ptr::copy_nonoverlapping;

use crate::pv::PvPacket;
use pnet::datalink::MacAddr;

pub fn gen_arp_response_packet(packet: &mut PvPacket, src_mac_address: &MacAddr) {
    const MAC_ADDRESS_LEN: usize = 6;
    const ARP_RSPN_PACKET_SIZE: u32 = 42;

    // copy dest MAC
    unsafe {
        copy_nonoverlapping(
            packet.buffer.offset((packet.start + 6) as isize),
            packet.buffer.offset(packet.start as isize),
            MAC_ADDRESS_LEN,
        );

        // copy src MAC
        std::ptr::write(
            packet.buffer.offset((packet.start + 6) as isize),
            src_mac_address.0,
        );
        std::ptr::write(
            packet.buffer.offset((packet.start + 7) as isize),
            src_mac_address.1,
        );
        std::ptr::write(
            packet.buffer.offset((packet.start + 8) as isize),
            src_mac_address.2,
        );
        std::ptr::write(
            packet.buffer.offset((packet.start + 9) as isize),
            src_mac_address.3,
        );
        std::ptr::write(
            packet.buffer.offset((packet.start + 10) as isize),
            src_mac_address.4,
        );
        std::ptr::write(
            packet.buffer.offset((packet.start + 11) as isize),
            src_mac_address.5,
        );

        // Ethertype - ARP
        std::ptr::write(packet.buffer.offset((packet.start + 12) as isize), 0x08);
        std::ptr::write(packet.buffer.offset((packet.start + 13) as isize), 0x06);

        // HW type
        std::ptr::write(packet.buffer.offset((packet.start + 14) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 15) as isize), 0x01);

        // protocol type
        std::ptr::write(packet.buffer.offset((packet.start + 16) as isize), 0x08);
        std::ptr::write(packet.buffer.offset((packet.start + 17) as isize), 0x00);

        // // HW size
        std::ptr::write(packet.buffer.offset((packet.start + 18) as isize), 0x06);

        // protocol size
        std::ptr::write(packet.buffer.offset((packet.start + 19) as isize), 0x04);

        // op code - ARP_REPLY
        std::ptr::write(packet.buffer.offset((packet.start + 20) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 21) as isize), 0x02);

        // sender MAC
        copy_nonoverlapping(
            packet.buffer.offset((packet.start + 6) as isize),
            packet.buffer.offset((packet.start + 22) as isize),
            MAC_ADDRESS_LEN,
        );

        // sender IP (10.0.0.4)
        std::ptr::write(packet.buffer.offset((packet.start + 28) as isize), 0x0a);
        std::ptr::write(packet.buffer.offset((packet.start + 29) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 30) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 31) as isize), 0x04);

        // target MAC
        copy_nonoverlapping(
            packet.buffer.offset((packet.start) as isize),
            packet.buffer.offset((packet.start + 32) as isize),
            MAC_ADDRESS_LEN,
        );

        // dest IP (10.0.0.5)
        std::ptr::write(packet.buffer.offset((packet.start + 38) as isize), 0x0a);
        std::ptr::write(packet.buffer.offset((packet.start + 39) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 40) as isize), 0x00);
        std::ptr::write(packet.buffer.offset((packet.start + 41) as isize), 0x05);

        // // set packet length of ARP response as 42 bytes
        packet.end = packet.start + ARP_RSPN_PACKET_SIZE;
    }
}
