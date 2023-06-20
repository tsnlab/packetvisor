/* ICMP example */
use clap::{arg, Command};
use num_derive::{FromPrimitive, ToPrimitive};
use packetvisor::pv;
use pnet::{
    datalink::{interfaces, MacAddr, NetworkInterface},
    packet::icmp::{IcmpTypes, MutableIcmpPacket},
    packet::MutablePacket,
    packet::{
        arp::{ArpHardwareTypes, ArpOperations, MutableArpPacket},
        ethernet::{EtherTypes, MutableEthernetPacket},
        ipv4,
    },
    packet::{icmp, ipv4::MutableIpv4Packet},
};
use signal_hook::SigId;
use std::{
    env,
    io::Error,
    net::Ipv4Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
};

#[derive(ToPrimitive, FromPrimitive)]
enum Protocol {
    ARP = 1,
    ICMP = 2,
    OTHER = 3,
}

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 5 {
        println!("check the number of arguments.");
        std::process::exit(-1);
    }

    let matches = Command::new("udp_example")
        .arg(arg!(interface: -i --interface <interface> "interface").required(true))
        .arg(arg!(chunk_size: -s --chunk_size <chunk_size> "chunk size").required(false).default_value("2048"))
        .arg(arg!(chunk_count: -c --chunk_count <chunk_count> "chunk count").required(false).default_value("1024"))
        .arg(arg!(rx_ring_size: -r --rx_ring_size <rx_ring_size> "rx ring size").required(false).default_value("64"))
        .arg(arg!(tx_ring_size: -t --tx_ring_size <tx_ring_size> "tx ring size").required(false).default_value("64"))
        .arg(arg!(filling_ring_size: -f --filling_ring_size <filling_ring_size> "filling ring size").required(false).default_value("64"))
        .arg(arg!(completion_ring_size: -o --completion_ring_size <completion_ring_size> "completion ring size").required(false).default_value("64"))
        .get_matches();

    let if_name = matches.get_one::<String>("interface").unwrap().clone();
    let chunk_size = (*matches.get_one::<String>("chunk_size").unwrap())
        .parse::<u32>()
        .expect("Check chunk size.");
    let chunk_count = (*matches.get_one::<String>("chunk_count").unwrap())
        .parse::<u32>()
        .expect("Check chunk count.");
    let rx_ring_size = (*matches.get_one::<String>("rx_ring_size").unwrap())
        .parse::<u32>()
        .expect("Check rx ring size.");
    let tx_ring_size = (*matches.get_one::<String>("tx_ring_size").unwrap())
        .parse::<u32>()
        .expect("Check tx ring size.");
    let filling_ring_size = (*matches.get_one::<String>("filling_ring_size").unwrap())
        .parse::<u32>()
        .expect("Check filling ring size.");
    let completion_ring_size = (*matches.get_one::<String>("completion_ring_size").unwrap())
        .parse::<u32>()
        .expect("Check completion ring size.");

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
    let pv_open_option: Option<pv::Nic> = pv::pv_open(
        &if_name,
        chunk_size,
        chunk_count,
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    );
    let mut nic: pv::Nic;
    if let Some(a) = pv_open_option {
        nic = a;
    } else {
        panic!("nic allocation failed!");
    }

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv::pv_receive(&mut nic, &mut packets, rx_batch_size);

        if received > 0 {
            let processed: u32 =
                process_packets(&mut nic, &mut packets, received, &src_mac_address);
            let sent: u32 = pv::pv_send(&mut nic, &mut packets, processed);

            if sent == 0 {
                for i in (0..processed).rev() {
                    pv::pv_free(&mut nic, &mut packets, i as usize);
                }
            }
        }
    }

    pv::pv_close(nic);
    println!("PV END");
}

fn process_packets(
    nic: &mut pv::Nic,
    packets: &mut Vec<pv::Packet>,
    batch_size: u32,
    src_mac_address: &MacAddr,
) -> u32 {
    let mut processed = 0;
    for i in (0..batch_size as usize).rev() {
        match get_protocol(&packets[i]) {
            Protocol::ARP => {
                if make_arp_response_packet(src_mac_address, &mut packets[i]).is_ok() {
                    processed += 1;
                } else {
                    pv::pv_free(nic, packets, i);
                }
            }
            Protocol::ICMP => {
                if make_icmp_response_packet(&mut packets[i]).is_ok() {
                    processed += 1;
                } else {
                    pv::pv_free(nic, packets, i);
                }
            }
            _ => {
                pv::pv_free(nic, packets, i);
            }
        }
    }
    processed
}

// analyze what kind of given packet
fn get_protocol(packet: &pv::Packet) -> Protocol {
    if is_icmp(packet) {
        Protocol::ICMP
    } else if is_arp(packet) {
        Protocol::ARP
    } else {
        Protocol::OTHER
    }
}

fn set_eth(packet: &mut MutableEthernetPacket) {
    let dest_mac_addr: MacAddr = packet.get_source();
    let src_mac_addr: MacAddr = packet.get_destination();

    packet.set_destination(dest_mac_addr);
    packet.set_source(src_mac_addr);
}

fn set_ipv4(packet: &mut MutableIpv4Packet) {
    let dest_ip_addr: Ipv4Addr = packet.get_source();
    let src_ip_addr: Ipv4Addr = packet.get_destination();

    packet.set_destination(dest_ip_addr);
    packet.set_source(src_ip_addr);
    packet.set_flags(0x00);
    packet.set_checksum(ipv4::checksum(&packet.to_immutable()));
}

fn set_icmp(packet: &mut MutableIcmpPacket) {
    packet.set_icmp_type(IcmpTypes::EchoReply);
    packet.set_checksum(icmp::checksum(&packet.to_immutable()));
}

fn make_icmp_response_packet(packet: &mut pv::Packet) -> Result<(), String> {
    let mut buffer: Vec<u8> = packet.get_buffer();

    let mut eth_pkt = MutableEthernetPacket::new(&mut buffer).unwrap();
    set_eth(&mut eth_pkt);
    let mut ip_pkt = MutableIpv4Packet::new(eth_pkt.payload_mut()).unwrap();
    set_ipv4(&mut ip_pkt);
    let mut icmp_pkt = MutableIcmpPacket::new(ip_pkt.payload_mut()).unwrap();
    set_icmp(&mut icmp_pkt);

    packet.replace_data(&buffer)
}

fn is_arp(packet: &pv::Packet) -> bool {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        std::ptr::read(payload_ptr.offset(12)) == 0x08 // Ethertype == 0x0806 (ARP)
            && std::ptr::read(payload_ptr.offset(13))                                                                                                                                                            == 0x06
            && std::ptr::read(payload_ptr.offset(20)) == 0x00 // arp.opcode = 0x0001
            && std::ptr::read(payload_ptr.offset(21)) == 0x01
    }
}

fn is_icmp(packet: &pv::Packet) -> bool {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        std::ptr::read(payload_ptr.offset(12)) == 0x08 // Ethertype == 0x0800 (IPv4)
            && std::ptr::read(payload_ptr.offset(13)) == 0x00
            && std::ptr::read(payload_ptr.offset(14)) >> 4 == 4    // IP version == 4
            && std::ptr::read(payload_ptr.offset(23)) == 0x01 // IPv4 Protocol == 0x01 (ICMP)
    }
}

fn make_arp_response_packet(src_mac_addr: &MacAddr, packet: &mut pv::Packet) -> Result<(), String> {
    let mut buffer: Vec<u8> = packet.get_buffer();

    let mut eth_pkt = MutableEthernetPacket::new(&mut buffer).unwrap();
    let dest_mac_addr: MacAddr = eth_pkt.get_source();
    eth_pkt.set_destination(dest_mac_addr);
    eth_pkt.set_source(*src_mac_addr);
    eth_pkt.set_ethertype(EtherTypes::Arp);

    let mut arp_req = MutableArpPacket::new(eth_pkt.payload_mut()).unwrap();
    let src_ipv4_addr = Ipv4Addr::new(10, 0, 0, 4); // 10.0.0.4
    let dest_ipv4_addr: Ipv4Addr = arp_req.get_sender_proto_addr();

    arp_req.set_hardware_type(ArpHardwareTypes::Ethernet);
    arp_req.set_protocol_type(EtherTypes::Ipv4);
    arp_req.set_hw_addr_len(6);
    arp_req.set_proto_addr_len(4);
    arp_req.set_operation(ArpOperations::Reply);
    arp_req.set_sender_hw_addr(*src_mac_addr);
    arp_req.set_sender_proto_addr(src_ipv4_addr);
    arp_req.set_target_hw_addr(dest_mac_addr);
    arp_req.set_target_proto_addr(dest_ipv4_addr);

    /* replace packet data with ARP packet */
    packet.replace_data(&buffer)
}
