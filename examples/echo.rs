use clap::{arg, value_parser, Command};
use pnet::{
    datalink::MacAddr,
    packet::arp::{ArpOperations, MutableArpPacket},
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        icmpv6::ndp::{MutableNeighborAdvertPacket, NdpOptionTypes, NeighborAdvertFlags},
        ip::IpNextHeaderProtocols,
        ipv4,
    },
    packet::{
        icmp::MutableIcmpPacket, icmpv6::MutableIcmpv6Packet, ipv4::MutableIpv4Packet,
        ipv6::MutableIpv6Packet,
    },
    packet::{
        icmp::{self, IcmpTypes},
        icmpv6::{self, Icmpv6Types},
        udp::{self, MutableUdpPacket},
        MutablePacket,
    },
};

use signal_hook::SigId;
use std::{
    io::Error,
    net::IpAddr,
    net::Ipv6Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let mut if_name: String = Default::default();
    let mut chunk_size: usize = 0;
    let mut chunk_count: usize = 0;
    let mut rx_ring_size: usize = 0;
    let mut tx_ring_size: usize = 0;
    let mut filling_ring_size: usize = 0;
    let mut completion_ring_size: usize = 0;

    do_command(
        &mut if_name,
        &mut chunk_size,
        &mut chunk_count,
        &mut rx_ring_size,
        &mut tx_ring_size,
        &mut filling_ring_size,
        &mut completion_ring_size,
    );

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

    let mut nic = match pv::Nic::new(
        &if_name,
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        tx_ring_size,
        rx_ring_size,
    ) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create Nic: {}", err);
        }
    };

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size = 64;
    while !term.load(Ordering::Relaxed) {
        thread::sleep(Duration::from_millis(100));

        let mut packets = nic.receive(rx_batch_size);
        let received = packets.len();

        if received == 0 && !term.load(Ordering::Relaxed) {
            continue;
        }

        packets.retain_mut(|p| process_packet(p, &nic));

        for retry in (0..3).rev() {
            match (nic.send(&mut packets), retry) {
                (cnt, _) if cnt > 0 => break, // Success
                (0, 0) => {
                    break; // Failed 3 times
                }
                _ => continue, // Retrying
            }
        }
    }
}

fn process_packet(packet: &mut pv::Packet, nic: &pv::Nic) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => {
            return false;
        }
    };

    // Swap source and destination
    eth.set_destination(eth.get_source());
    eth.set_source(nic.interface.mac.unwrap());

    match eth.get_ethertype() {
        EtherTypes::Arp => process_arp(packet, &nic.interface.mac.unwrap()),
        EtherTypes::Ipv4 => process_ipv4(packet, nic),
        EtherTypes::Ipv6 => process_ipv6(packet, nic),
        _ => false,
    }
}

fn process_arp(packet: &mut pv::Packet, my_mac: &MacAddr) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut arp = MutableArpPacket::new(eth.payload_mut()).unwrap();

    if arp.get_operation() != ArpOperations::Request {
        return false;
    }

    let target_ip = arp.get_target_proto_addr();

    arp.set_operation(ArpOperations::Reply);
    arp.set_target_hw_addr(arp.get_sender_hw_addr());
    arp.set_target_proto_addr(arp.get_target_proto_addr());
    arp.set_sender_hw_addr(*my_mac);
    arp.set_sender_proto_addr(target_ip);

    true
}

fn process_ipv4(packet: &mut pv::Packet, nic: &pv::Nic) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();

    let my_ip = &nic
        .interface
        .ips
        .iter()
        .find(|ip| ip.is_ipv4())
        .expect("not allocated ipv4 to interface")
        .ip();

    if ipv4.get_destination() != *my_ip {
        return false;
    }

    ipv4.set_destination(ipv4.get_source());
    ipv4.set_source(my_ip.to_string().parse().unwrap());

    match ipv4.get_next_level_protocol() {
        IpNextHeaderProtocols::Icmp => process_icmp(packet),
        IpNextHeaderProtocols::Udp => process_udp(packet),
        _ => false,
    }
}

fn process_icmp(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let mut icmp = MutableIcmpPacket::new(ipv4.payload_mut()).unwrap();

    if icmp.get_icmp_type() != IcmpTypes::EchoRequest {
        return false;
    }

    icmp.set_icmp_type(IcmpTypes::EchoReply);
    icmp.set_checksum(icmp::checksum(&icmp.to_immutable()));
    ipv4.set_checksum(ipv4::checksum(&ipv4.to_immutable()));

    true
}

fn process_udp(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let source = ipv4.get_source();
    let destination = ipv4.get_destination();
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();

    if udp.get_destination() != 7 {
        return false;
    }

    let src_port = udp.get_source();

    udp.set_source(udp.get_destination());
    udp.set_destination(src_port);
    udp.set_checksum(udp::ipv4_checksum(
        &udp.to_immutable(),
        &source,
        &destination,
    ));
    ipv4.set_checksum(ipv4::checksum(&ipv4.to_immutable()));

    true
}

fn process_ipv6(packet: &mut pv::Packet, nic: &pv::Nic) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let my_ip = &nic
        .interface
        .ips
        .iter()
        .find(|ip| ip.is_ipv6())
        .expect("not allocated ipv6 to interface")
        .ip();

    let ipv6_addr = match my_ip {
        IpAddr::V6(ipv6) => ipv6,
        _ => panic!("not ipv6"),
    };

    ipv6.set_destination(ipv6.get_source());
    ipv6.set_source(*ipv6_addr);
    match ipv6.get_next_header() {
        IpNextHeaderProtocols::Udp => process_udpv6(packet),
        IpNextHeaderProtocols::Icmpv6 => process_icmpv6(packet, ipv6_addr),
        _ => false,
    }
}

fn process_icmpv6(packet: &mut pv::Packet, ipv6_addr: &Ipv6Addr) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let other_ipv6 = ipv6.get_destination();
    let mut icmpv6 = MutableIcmpv6Packet::new(ipv6.payload_mut()).unwrap();

    if icmpv6.get_icmpv6_type() == Icmpv6Types::NeighborSolicit {
        process_ndp(packet, ipv6_addr);
        return true;
    } else if icmpv6.get_icmpv6_type() == Icmpv6Types::EchoRequest {
        icmpv6.set_icmpv6_type(Icmpv6Types::EchoReply);
        let checksum = icmpv6::checksum(&icmpv6.to_immutable(), ipv6_addr, &other_ipv6);
        icmpv6.set_checksum(checksum);
        return true;
    }
    false
}

fn process_ndp(packet: &mut pv::Packet, ipv6_addr: &Ipv6Addr) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let eth_addr = eth.get_source();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let other_ipv6 = ipv6.get_destination();

    let mut icmpv6_ndp = MutableNeighborAdvertPacket::new(ipv6.payload_mut()).unwrap();
    let ndp_flag = NeighborAdvertFlags::Solicited | NeighborAdvertFlags::Override;
    icmpv6_ndp.set_flags(ndp_flag);

    let mut ndp_option = icmpv6_ndp.get_options();
    //Copy sender's mac_addr to ndp_option_data
    for i in 0..6 {
        ndp_option[0].data[i] = eth_addr.octets()[i];
    }
    ndp_option[0].option_type = NdpOptionTypes::TargetLLAddr;
    icmpv6_ndp.set_options(&ndp_option);

    let mut icmpv6 = MutableIcmpv6Packet::new(ipv6.payload_mut()).unwrap();
    icmpv6.set_icmpv6_type(Icmpv6Types::NeighborAdvert);
    let checksum = icmpv6::checksum(&icmpv6.to_immutable(), ipv6_addr, &other_ipv6);
    icmpv6.set_checksum(checksum);
    true
}

fn process_udpv6(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let source = ipv6.get_source();
    let destination = ipv6.get_destination();
    let mut udp = MutableUdpPacket::new(ipv6.payload_mut()).unwrap();

    if udp.get_destination() != 7 {
        return false;
    }
    let src_port = udp.get_source();
    udp.set_source(udp.get_destination());
    udp.set_destination(src_port);

    udp.set_checksum(udp::ipv6_checksum(
        &udp.to_immutable(),
        &source,
        &destination,
    ));
    true
}

fn do_command(
    if_name: &mut String,
    chunk_size: &mut usize,
    chunk_count: &mut usize,
    rx_ring_size: &mut usize,
    tx_ring_size: &mut usize,
    filling_ring_size: &mut usize,
    completion_ring_size: &mut usize,
) {
    let matches = Command::new("echo")
        .arg(arg!(interface: <interface> "Interface to use").required(true))
        .arg(
            arg!(chunk_size: -s --"chunk-size" <size> "Chunk size")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("2048"),
        )
        .arg(
            arg!(chunk_count: -c --"chunk-count" <count> "Chunk count")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("1024"),
        )
        .arg(
            arg!(rx_ring_size: -r --"rx-ring" <count> "Rx ring size")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .required(false)
                .value_parser(value_parser!(usize))
                .default_value("64"),
        )
        .get_matches();

    *if_name = matches.get_one::<String>("interface").unwrap().clone();
    *chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    *chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    *rx_ring_size = *matches.get_one::<usize>("rx_ring_size").unwrap();
    *tx_ring_size = *matches.get_one::<usize>("tx_ring_size").unwrap();
    *filling_ring_size = *matches.get_one::<usize>("fill_ring_size").unwrap();
    *completion_ring_size = *matches.get_one::<usize>("completion_ring_size").unwrap();
}
