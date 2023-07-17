use clap::{arg, Command};
use pnet::{
    datalink::MacAddr,
    packet::arp::{ArpOperations, MutableArpPacket},
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        icmpv6::ndp::MutableNeighborAdvertPacket,
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
    net::Ipv6Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let matches = Command::new("echo")
        .arg(arg!(interface: -i --interface <interface> "Interface to use").required(true))
        .arg(
            arg!(chunk_size: -s --"chunk-size" <size> "Chunk size")
                .required(false)
                .default_value("2048"),
        )
        .arg(
            arg!(chunk_count: -c --"chunk-count" <count> "Chunk count")
                .required(false)
                .default_value("1024"),
        )
        .arg(
            arg!(rx_ring_size: -r --"rx-ring" <count> "Rx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    let if_name = matches.get_one::<String>("interface").unwrap().clone();
    let chunk_size = matches
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

        while received == 0 && !term.load(Ordering::Relaxed) {
            // No packets received. Sleep
            thread::sleep(Duration::from_millis(100));
            received = nic.receive(&mut packets);
        }

        for i in 0..received as usize {
            match process_packet(&mut packets[i], &nic) {
                true => {}
                false => {
                    nic.free(&mut packets[i]);
                    packets.remove(i);
                }
            }
        }

        for retry in (0..3).rev() {
            match (nic.send(&mut packets), retry) {
                (cnt, _) if cnt > 0 => break, // Success
                (0, 0) => {
                    break; // Failed 3 times
                }
                _ => continue, // Retrying
            }
        }

        packets.clear();
    }

    nic.close();
}

fn process_packet(packet: &mut pv::Packet, nic: &pv::NIC) -> bool {
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
        EtherTypes::Ipv4 => process_ipv4(packet, &nic),
        EtherTypes::Ipv6 => process_ipv6(packet, &nic),
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

fn process_ipv4(packet: &mut pv::Packet, nic: &pv::NIC) -> bool {
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

fn process_ipv6(packet: &mut pv::Packet, nic: &pv::NIC) -> bool {
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
    let v6: Ipv6Addr = my_ip.to_string().parse().unwrap();

    ipv6.set_destination(ipv6.get_source());
    ipv6.set_source(v6);

    match ipv6.get_next_header() {
        IpNextHeaderProtocols::Udp => process_udpv6(packet),
        IpNextHeaderProtocols::Icmpv6 => process_icmpv6(packet, &v6),
        _ => false,
    }
}

fn process_icmpv6(packet: &mut pv::Packet, v6: &Ipv6Addr) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let other_ipv6 = ipv6.get_destination();
    let mut icmpv6 = MutableIcmpv6Packet::new(ipv6.payload_mut()).unwrap();

    if icmpv6.get_icmpv6_type() == Icmpv6Types::NeighborSolicit {
        process_ndp(packet, &v6);
        return true;
    } else if icmpv6.get_icmpv6_type() == Icmpv6Types::EchoRequest {
        icmpv6.set_icmpv6_type(Icmpv6Types::EchoReply);
        let checksum = icmpv6::checksum(&icmpv6.to_immutable(), &v6, &other_ipv6);
        icmpv6.set_checksum(checksum);
        return true;
    }
    false
}

fn process_ndp(packet: &mut pv::Packet, v6: &Ipv6Addr) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    let eth_addr = eth.get_source();
    let ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();
    let other_ipv6 = ipv6.get_destination();
    let mut ipv6 = MutableIpv6Packet::new(eth.payload_mut()).unwrap();

    let mut icmpv6_ndp = MutableNeighborAdvertPacket::new(ipv6.payload_mut()).unwrap();
    icmpv6_ndp.set_flags(0x60);

    let mut ndp_option = icmpv6_ndp.get_options();
    for i in 0..6 {
        ndp_option[0].data[i] = eth_addr.octets()[i];
    }
    icmpv6_ndp.set_options(&ndp_option);

    let mut icmpv6 = MutableIcmpv6Packet::new(ipv6.payload_mut()).unwrap();
    icmpv6.set_icmpv6_type(Icmpv6Types::NeighborAdvert);
    let checksum = icmpv6::checksum(&icmpv6.to_immutable(), &v6, &other_ipv6);
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
