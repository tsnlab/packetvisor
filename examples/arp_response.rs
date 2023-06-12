/* ARP example */

use pnet::{
    datalink::{interfaces, MacAddr, NetworkInterface},
    packet::arp::*,
    packet::ethernet::{EtherTypes, MutableEthernetPacket},
    packet::{MutablePacket, Packet},
};
use pv::pv::*;
use signal_hook::SigId;
use std::{
    env,
    io::Error,
    net::Ipv4Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
};

enum PacketKind {
    ArpReq,
    Unused,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 8 {
        println!("check the number of arguments.");
        std::process::exit(-1);
    }

    let mut if_name = String::new();
    let mut chunk_size: u32 = 0;
    let mut chunk_count: u32 = 0;
    let mut rx_ring_size: u32 = 0;
    let mut tx_ring_size: u32 = 0;
    let mut filling_ring_size: u32 = 0;
    let mut completion_ring_size: u32 = 0;

    for i in 1..args.len() {
        match i {
            1 => {
                if_name = args[1].clone();
            }
            2 => {
                chunk_size = args[2].parse::<u32>().expect("Check chunk size.");
            }
            3 => {
                chunk_count = args[3].parse::<u32>().expect("Check chunk count.");
            }
            4 => {
                rx_ring_size = args[4].parse::<u32>().expect("Check rx ring size.");
            }
            5 => {
                tx_ring_size = args[5].parse::<u32>().expect("Check tx ring size.");
            }
            6 => {
                filling_ring_size = args[6].parse::<u32>().expect("Check filling ring size.");
            }
            7 => {
                completion_ring_size = args[7].parse::<u32>().expect("Check filling ring size.");
            }
            _ => {
                panic!("abnormal index");
            }
        }
    }

    /* signal define to end the application */
    let term: Arc<AtomicBool> = Arc::new(AtomicBool::new(false));
    let result_sigint: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGINT, Arc::clone(&term));
    let result_sigterm: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGTERM, Arc::clone(&term));
    let result_sigabrt: Result<SigId, Error> =
        signal_hook::flag::register(signal_hook::consts::SIGABRT, Arc::clone(&term));

    if result_sigint.or(result_sigterm).or(result_sigabrt).is_err() {
        panic!("signal is forbidden");
    }

    /* save source MAC address into variable */
    let all_interfaces: Vec<NetworkInterface> = interfaces();
    let is_exist: Option<&NetworkInterface> = all_interfaces
        .iter()
        .find(|element| element.name.as_str() == if_name.as_str());

    let src_mac_address: MacAddr = match is_exist {
        Some(target_interface) => {
            let option: Option<MacAddr> = target_interface.mac;
            if option.is_none() {
                panic!("couldn't find source MAC address");
            }

            option.unwrap()
        }
        None => {
            panic!("couldn't find source MAC address");
        }
    };

    /* execute pv_open() */
    let pv_open_option: Option<PvNic> = pv::pv::pv_open(
        &if_name,
        chunk_size,
        chunk_count,
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    );
    let mut nic: PvNic;
    if let Some(a) = pv_open_option {
        nic = a;
    } else {
        panic!("nic allocation failed!");
    }

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<PvPacket> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv_receive(&mut nic, &mut packets, rx_batch_size);

        if received > 0 {
            let processed: u32 =
                process_packets(&mut nic, &mut packets, received, &src_mac_address);
            let sent: u32 = pv_send(&mut nic, &mut packets, processed);

            if sent == 0 {
                for i in (0..processed).rev() {
                    pv_free(&mut nic, &mut packets, i as usize);
                }
            }
        }
    }

    pv::pv::pv_close(nic);
    println!("PV END");
}

fn process_packets(
    nic: &mut PvNic,
    packets: &mut Vec<PvPacket>,
    batch_size: u32,
    src_mac_address: &MacAddr,
) -> u32 {
    let mut processed = 0;
    for i in (0..batch_size).rev() {
        let packet_kind: PacketKind = packet_kind_checker(&packets[i as usize]);
        // analyze packet and create packet

        let processing_result: Result<(), ()>;
        // process packet
        match packet_kind {
            PacketKind::ArpReq => {
                processing_result =
                    gen_arp_response_packet(src_mac_address, &mut packets[i as usize]);
            }
            _ => {
                processing_result = Err(());
            }
        }

        if processing_result.is_ok() {
            processed += 1;
        } else {
            pv_free(nic, packets, i as usize);
        }
    }
    processed
}

// analyze what kind of given packet
fn packet_kind_checker(packet: &PvPacket) -> PacketKind {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        if std::ptr::read(payload_ptr.offset(12)) == 0x08
            && std::ptr::read(payload_ptr.offset(13)) == 0x06
            && std::ptr::read(payload_ptr.offset(20)) == 0x00
            && std::ptr::read(payload_ptr.offset(21)) == 0x01
        // ARP request packet
        {
            return PacketKind::ArpReq;
        }
    }

    PacketKind::Unused
}

fn gen_arp_response_packet(src_mac_addr: &MacAddr, packet: &mut PvPacket) -> Result<(), ()> {
    let src_ipv4_addr = Ipv4Addr::new(10, 0, 0, 4); // 10.0.0.4
    let dest_mac_addr: MacAddr = unsafe {
        MacAddr::new(
            std::ptr::read(packet.buffer.offset((packet.start + 22) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 23) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 24) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 25) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 26) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 27) as isize)),
        )
    };

    let dest_ipv4_addr = unsafe {
        Ipv4Addr::new(
            std::ptr::read(packet.buffer.offset((packet.start + 28) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 29) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 30) as isize)),
            std::ptr::read(packet.buffer.offset((packet.start + 31) as isize)),
        )
    };

    /* make ARP response packet header */
    let mut arp_buffer = [0u8; 28];
    let mut arp_packet = MutableArpPacket::new(&mut arp_buffer).unwrap();

    arp_packet.set_hardware_type(ArpHardwareTypes::Ethernet);
    arp_packet.set_protocol_type(EtherTypes::Ipv4);
    arp_packet.set_hw_addr_len(6);
    arp_packet.set_proto_addr_len(4);
    arp_packet.set_operation(ArpOperations::Reply);
    arp_packet.set_sender_hw_addr(*src_mac_addr);
    arp_packet.set_sender_proto_addr(src_ipv4_addr);
    arp_packet.set_target_hw_addr(dest_mac_addr);
    arp_packet.set_target_proto_addr(dest_ipv4_addr);

    /* attach ethernet header to ARP header */
    let mut ether_buffer = [0u8; 42];
    let mut ether_packet = MutableEthernetPacket::new(&mut ether_buffer).unwrap();
    ether_packet.set_destination(dest_mac_addr);
    ether_packet.set_source(*src_mac_addr);
    ether_packet.set_ethertype(EtherTypes::Arp);
    ether_packet.set_payload(arp_packet.packet_mut());

    /* replace packet data with ARP packet */
    packet.replace_data(&ether_packet.packet().to_vec());

    Ok(())
}
