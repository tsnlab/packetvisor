use clap::{arg, Command};

use pnet::{
    datalink::MacAddr,
    packet::{
        arp::{ArpOperations, MutableArpPacket},
        PacketSize,
    },
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
    },
    packet::{icmp::MutableIcmpPacket, ipv4::MutableIpv4Packet},
    packet::{
        icmp::{self, IcmpTypes},
        ipv4,
        udp::{self, MutableUdpPacket},
        MutablePacket,
    },
};
use signal_hook::SigId;
use std::{
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
};

fn main() {
    let matches = Command::new("echo")
        .arg(arg!(interface: -i --interface <interface> "Interface to use").required(true))
        .arg(
            arg!(chunk_size: -s --chunk-size <size> "Chunk size")
                .required(false)
                .default_value("2048"),
        )
        .arg(
            arg!(chunk_count: -c --chunk-count <count> "Chunk count")
                .required(false)
                .default_value("1024"),
        )
        .arg(
            arg!(rx_ring_size: -r --rx-ring <count> "Rx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --tx-ring <count> "Tx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --fill-ring <count> "Fill ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --completion-ring <count> "Completion ring size")
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    let if_name = matches.get_one::<String>("interface").unwrap().clone();
    let chunk_size: u32 = matches
        .get_one::<String>("chunk_size")
        .unwrap()
        .parse()
        .unwrap();
    let chunk_count = matches
        .get_one::<String>("chunk_count")
        .unwrap()
        .parse()
        .unwrap();
    let rx_ring_size = matches
        .get_one::<String>("rx_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let tx_ring_size = matches
        .get_one::<String>("tx_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let filling_ring_size = matches
        .get_one::<String>("fill_ring_size")
        .unwrap()
        .parse()
        .unwrap();
    let completion_ring_size = matches
        .get_one::<String>("completion_ring_size")
        .unwrap()
        .parse()
        .unwrap();

    // Signal handlers
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

    let mut nic = pv::NIC::new(&if_name, chunk_size, chunk_count).unwrap();
    nic.open(
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    )
    .unwrap();

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let mut received = nic.receive(&mut packets);

        if received == 0 {
            // No packets received. Sleep
            // thread::sleep(Duration::from_millis(100));
            continue;
        }

        for i in (0..received as usize).rev() {
            match process_packet(&mut packets[i], &nic.interface.mac.unwrap()) {
                Some(n) => {
                    packets[i].resize(n as u32);
                }
                None => {
                    nic.free(&mut packets[i]);
                    packets.remove(i);
                    received -= 1;
                }
            }
        }

        match nic.send(&mut packets) {
            0 => {
                for i in (0..received as usize).rev() {
                    nic.free(&mut packets[i]);
                    packets.remove(i);
                }
            }
            _ => {}
        }
    }

    nic.close();
}

fn process_packet(packet: &mut pv::Packet, my_mac: &MacAddr) -> Option<usize> {
    let buffer = packet.get_buffer_mut();
    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => {
            return None;
        }
    };

    // Swap source and destination
    eth.set_destination(eth.get_source());
    eth.set_source(*my_mac);

    match eth.get_ethertype() {
        EtherTypes::Arp => {
            // println!("Got ARP packet");
            process_arp(&mut eth, my_mac)
        }
        EtherTypes::Ipv4 => process_ipv4(&mut eth),
        _ => None,
    }
}

fn process_arp(eth: &mut MutableEthernetPacket, my_mac: &MacAddr) -> Option<usize> {
    let mut arp = MutableArpPacket::new(eth.payload_mut()).unwrap();

    if arp.get_operation() != ArpOperations::Request {
        return None;
    }

    let target_ip = arp.get_target_proto_addr();

    arp.set_operation(ArpOperations::Reply);
    arp.set_target_hw_addr(arp.get_sender_hw_addr());
    arp.set_target_proto_addr(arp.get_target_proto_addr());
    arp.set_sender_hw_addr(*my_mac);
    arp.set_sender_proto_addr(target_ip);

    Some(arp.packet_size() + eth.packet_size())
}

fn process_ipv4(eth: &mut MutableEthernetPacket) -> Option<usize> {
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();

    let tmp = ipv4.get_source();
    ipv4.set_source(ipv4.get_destination());
    ipv4.set_destination(tmp);

    let ret = match ipv4.get_next_level_protocol() {
        IpNextHeaderProtocols::Udp => process_udp(&mut ipv4),
        IpNextHeaderProtocols::Icmp => process_icmp(&mut ipv4),
        _ => None,
    };

    ipv4.set_checksum(ipv4::checksum(&ipv4.to_immutable()));

    match ret {
        Some(n) => Some(n + eth.packet_size()),
        None => None,
    }
}

fn process_icmp(ipv4: &mut MutableIpv4Packet) -> Option<usize> {
    let mut icmp = MutableIcmpPacket::new(ipv4.payload_mut()).unwrap();

    if icmp.get_icmp_type() != IcmpTypes::EchoRequest {
        return None;
    }

    icmp.set_icmp_type(IcmpTypes::EchoReply);
    icmp.set_checksum(icmp::checksum(&icmp.to_immutable()));

    Some(icmp.packet_size() + ipv4.packet_size())
}

fn process_udp(ipv4: &mut MutableIpv4Packet) -> Option<usize> {
    let source = ipv4.get_source();
    let destination = ipv4.get_destination();
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();

    if udp.get_destination() != 7 {
        return None;
    }

    let tmp = udp.get_source();

    udp.set_source(udp.get_destination());
    udp.set_destination(tmp);
    udp.set_checksum(udp::ipv4_checksum(
        &udp.to_immutable(),
        &source,
        &destination,
    ));

    Some(udp.packet_size() + ipv4.packet_size())
}