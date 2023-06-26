use clap::{arg, Command};
use packetvisor::pv;
use pnet::{
    datalink::{interfaces, MacAddr, NetworkInterface},
    packet::{
        arp::{ArpHardwareTypes, ArpOperations, MutableArpPacket},
        ethernet::{EtherTypes, MutableEthernetPacket},
        PacketSize
    },
    packet::{
        ipv4::{self, MutableIpv4Packet},
        tcp::{self, MutableTcpPacket},
        MutablePacket,
    },
};
use signal_hook::SigId;
use std::{
    io::Error,
    net::Ipv4Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc
};

#[derive(PartialEq)]
enum Protocol {
    ARP,
    TCP,
    OTHER,
}

fn main() {
    let matches = Command::new("tcp_example")
        .arg(arg!(interface: -i --interface <interface> "interface").required(true))
        .arg(arg!(destination: -d --destination <destination> "destination").required(true))
        .arg(arg!(port: -p --port <port> "port").required(true))
        .arg(arg!(chunk_size: -s --chunk_size <chunk_size> "chunk size").required(false).default_value("2048"))
        .arg(arg!(chunk_count: -c --chunk_count <chunk_count> "chunk count").required(false).default_value("1024"))
        .arg(arg!(rx_ring_size: -r --rx_ring_size <rx_ring_size> "rx ring size").required(false).default_value("64"))
        .arg(arg!(tx_ring_size: -t --tx_ring_size <tx_ring_size> "tx ring size").required(false).default_value("64"))
        .arg(arg!(filling_ring_size: -f --filling_ring_size <filling_ring_size> "filling ring size").required(false).default_value("64"))
        .arg(arg!(completion_ring_size: -o --completion_ring_size <completion_ring_size> "completion ring size").required(false).default_value("64"))
        .get_matches();

    /*
    #########################################################################################
    ############ change the network to make two namespaces each of which has a NIC ##########
    #################### when receive packet by nic2, send it to nic1  ######################
    #########################################################################################
    */
    let if_name = matches.get_one::<String>("interface").unwrap().to_string();
    let dest_if = matches.get_one::<String>("destination").unwrap().to_string();

    let port = (*matches.get_one::<String>("port").unwrap())
        .parse::<u32>()
        .expect("Check port.");
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
    let matched_interface: Option<&NetworkInterface> = all_interfaces
        .iter()
        .find(|element| element.name.as_str() == if_name.as_str());

    let src_mac_address: MacAddr = match matched_interface {
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
    let mut nic1: pv::Nic;
    if let Some(a) = pv_open_option {
        nic1 = a;
    } else {
        panic!("nic allocation failed!");
    }

    let is_exist: Option<&NetworkInterface> = all_interfaces
        .iter()
        .find(|element| element.name.as_str() == dest_if.as_str());
    if is_exist.is_none() {
        panic!("couldn't find dest MAC address");
    }
    let pv_open_option: Option<pv::Nic> = pv::pv_open(
        &dest_if,
        chunk_size,
        chunk_count,
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    );
    let mut nic2: pv::Nic;
    if let Some(a) = pv_open_option {
        nic2 = a;
    } else {
        panic!("nic allocation failed!");
    }

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv::pv_receive(&mut nic1, &mut packets, rx_batch_size);
        if received > 0 {
            let processed: u32 = process_packets(
                &mut nic1,
                &mut packets,
                received,
                &src_mac_address,
                port,
            );
            let mut copy_packets = Vec::new();
            for i in (0..processed).rev() {
                let mut copy_packet = pv::pv_alloc(&mut nic2).unwrap();
                copy_packet.replace_data(&packets[i as usize].get_buffer_mut().to_vec()).unwrap();
                copy_packets.push(copy_packet);
            }
            let sent: u32 = pv::pv_send(&mut nic2, &mut copy_packets, processed);
            if sent == 0 {
                for i in (0..processed).rev() {
                    pv::pv_free(&mut nic1, &mut packets, i as usize);
                }
            }
        }
        let received: u32 = pv::pv_receive(&mut nic2, &mut packets, rx_batch_size);
        if received > 0 {
            let processed: u32 = process_packets(
                &mut nic1,
                &mut packets,
                received,
                &src_mac_address,
                port,
            );
            let mut copy_packets = Vec::new();
            for i in (0..processed).rev() {
                let mut copy_packet = pv::pv_alloc(&mut nic1).unwrap();
                copy_packet.replace_data(&packets[i as usize].get_buffer_mut().to_vec()).unwrap();
                copy_packets.push(copy_packet);
            }
            let sent: u32 = pv::pv_send(&mut nic1, &mut copy_packets, processed);
            if sent == 0 {
                for i in (0..processed).rev() {
                    pv::pv_free(&mut nic2, &mut packets, i as usize);
                }
            }
        }
    }

    pv::pv_close(nic1);
    pv::pv_close(nic2);

    println!("PV END");
}

fn process_packets(
    nic: &mut pv::Nic,
    packets: &mut Vec<pv::Packet>,
    batch_size: u32,
    src_mac_address: &MacAddr,
    port: u32,
) -> u32 {
    let mut processed = 0;
    for i in (0..batch_size as usize).rev() {
        // packet_dump(&packets[i]);
        if get_protocol(&packets[i]) == Protocol::ARP {

        }

        match get_protocol(&packets[i]) {
            Protocol::ARP => {
                println!("arp");
                // processed += 1;
                if make_arp_response_packet(src_mac_address, &mut packets[i]).is_ok() {
                    processed += 1;
                } else {
                    pv::pv_free(nic, packets, i);
                }
            }
            Protocol::TCP => {
                if chk_port(&packets[i], port) {
                    // if make_tcp_response_packet(&mut packets[i], dest).is_ok() {
                        processed += 1;
                    // } else {
                    //     pv::pv_free(nic, packets, i);
                    // }
                    // // processed += 1;
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

fn chk_port(packet: &pv::Packet, port: u32) -> bool {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        let mut port_bytes: [u8; 2] = [0; 2];
        port_bytes[0] = std::ptr::read(payload_ptr.offset(36));
        port_bytes[1] = std::ptr::read(payload_ptr.offset(37));
        let packet_port: u16 = u16::from_be_bytes(port_bytes);
        packet_port as u32 == port
    }
}

// analyze what kind of given packet
fn get_protocol(packet: &pv::Packet) -> Protocol {
    if is_tcp(packet) {
        Protocol::TCP
    } else if is_arp(packet) {
        Protocol::ARP
    } else {
        Protocol::OTHER
    }
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

fn is_tcp(packet: &pv::Packet) -> bool {
    let payload_ptr = unsafe { packet.buffer.add(packet.start as usize).cast_const() };

    unsafe {
        std::ptr::read(payload_ptr.offset(12)) == 0x08 // Ethertype == 0x0800 (IPv4)
            && std::ptr::read(payload_ptr.offset(13)) == 0x00
            && std::ptr::read(payload_ptr.offset(14)) >> 4 == 4    // IP version == 4
            && std::ptr::read(payload_ptr.offset(23)) == 0x06 // IPv4 Protocol == 0x06 (TCP)
    }
}

fn make_tcp_response_packet(packet: &mut pv::Packet, dest: Ipv4Addr) -> Result<(), String> {
    let mut buffer = packet.get_buffer_mut();

    let eth_pkt = MutableEthernetPacket::new(&mut buffer);
    if eth_pkt.is_none() {
        return Err(String::from(
            "buffer size is less than the minimum required packet size"
        ));
    }
    let mut eth_pkt = eth_pkt.unwrap();
    set_eth(&mut eth_pkt);
    let ip_pkt = MutableIpv4Packet::new(eth_pkt.payload_mut());
    if ip_pkt.is_none() {
        return Err(String::from(
            "buffer size is less than the minimum required packet size"
        ));
    }
    let mut ip_pkt = ip_pkt.unwrap();
    set_ipv4(&mut ip_pkt, dest);
    let source = ip_pkt.get_source();
    let destination = ip_pkt.get_destination();
    let tcp_pkt = MutableTcpPacket::new(ip_pkt.payload_mut());
    if tcp_pkt.is_none() {
        return Err(String::from(
            "buffer size is less than the minimum required packet size"
        ));
    }
    let mut tcp_pkt = tcp_pkt.unwrap();
    set_tcp(&mut tcp_pkt);
    tcp_pkt.set_checksum(tcp::ipv4_checksum(
        &tcp_pkt.to_immutable(),
        &source,
        &destination,
    ));
    Ok(())
}

fn set_eth(packet: &mut MutableEthernetPacket) {
    let dest_mac_addr: MacAddr = packet.get_source();
    let src_mac_addr: MacAddr = packet.get_destination();

    packet.set_destination(dest_mac_addr);
    packet.set_source(src_mac_addr);
}

fn set_ipv4(packet: &mut MutableIpv4Packet, dest: Ipv4Addr) {
    packet.set_source(packet.get_destination());
    packet.set_destination(dest);
    packet.set_checksum(ipv4::checksum(&packet.to_immutable()));
}

fn set_tcp(packet: &mut MutableTcpPacket) {
    let dest_port: u16 = packet.get_source();
    let src_port: u16 = packet.get_destination();

    packet.set_destination(dest_port);
    packet.set_source(src_port);
    packet.set_flags(0x012);
}

fn make_arp_response_packet(src_mac_addr: &MacAddr, packet: &mut pv::Packet) -> Result<(), String> {
    let mut buffer = packet.get_buffer_mut();

    let eth_pkt = MutableEthernetPacket::new(&mut buffer);
    if eth_pkt.is_none() {
        return Err(String::from(
            "buffer size is less than the minimum required packet size",
        ));
    }
    let mut eth_pkt = eth_pkt.unwrap();
    let dest_mac_addr: MacAddr = eth_pkt.get_source();
    eth_pkt.set_destination(dest_mac_addr);
    eth_pkt.set_source(*src_mac_addr);
    eth_pkt.set_ethertype(EtherTypes::Arp);

    let arp_req = MutableArpPacket::new(eth_pkt.payload_mut());
    if arp_req.is_none() {
        return Err(String::from(
            "buffer size is less than the minimum required packet size",
        ));
    }
    let mut arp_req = arp_req.unwrap();
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

    let packet_size = (arp_req.packet_size() + eth_pkt.packet_size()) as u32;
    packet.end = packet.start + packet_size;

    Ok(())
}


fn packet_dump(packet: &pv::Packet) {
    let buffer_address: *const u8 = packet.buffer.cast_const();

    let length: u32 = packet.end - packet.start;
    let mut count: u32 = 0;

    unsafe {
        loop {
            let read_offset: usize = count as usize;
            let read_address: *const u8 = buffer_address.add(packet.start as usize + read_offset);
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
