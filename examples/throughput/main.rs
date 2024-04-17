#![allow(static_mut_refs)]

use clap::{arg, Command};

use signal_hook::SigId;
use std::{
    io::Error,
    net::{IpAddr, Ipv4Addr},
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::{Duration, Instant},
};

use pnet::{
    datalink,
    datalink::{MacAddr, NetworkInterface},
    packet::{
        ethernet::{EtherType, EtherTypes, EthernetPacket, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        ipv4,
        ipv4::{Ipv4Packet, MutableIpv4Packet},
        udp::{MutableUdpPacket, UdpPacket},
        {MutablePacket, Packet},
    },
};

static mut STATS_RUNNING: bool = false;

struct Statistics {
    pkt_count: usize,
    total_bytes: usize,
    duration: usize,
}

static mut STATS: Statistics = Statistics {
    pkt_count: 0,
    total_bytes: 0,
    duration: 0,
};

const CHUNK_SIZE: usize = 2048;
const CHUNK_COUNT: usize = 1024;
const FILL_RING_SIZE: usize = 256;
const COMP_RING_SIZE: usize = 256;
const TX_RING_SIZE: usize = 256;
const RX_RING_SIZE: usize = 256;

fn main() {
    let receiver_command = Command::new("receiver")
        .about("Receiver mode")
        .arg(arg!(iface: -i <iface> "Interface to use").required(true))
        .arg(arg!(src_port: <source_port> "Source Port Number").required(true));

    let sender_command = Command::new("sender")
        .about("Sender mode")
        .arg(arg!(iface: -i <iface> "Interface to use").required(true))
        .arg(arg!(dest_mac: <destination_mac> "Destination MAC Address").required(true))
        .arg(arg!(dest_ip: <destination_ip> "Destination IP Address").required(true))
        .arg(arg!(dest_port: <destination_port> "Destination Port Number").required(true))
        .arg(
            arg!(payload: -b <udp_payload_size> "UDP Payload Size(Byte) - default 1440Bytes")
                .required(false)
                .default_value("1440"),
        );

    let matched_command = Command::new("throughput")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(receiver_command)
        .subcommand(sender_command)
        .get_matches();

    match matched_command.subcommand() {
        Some(("receiver", sub_matches)) => {
            let iface = sub_matches.get_one::<String>("iface").unwrap().to_string();
            let src_port = sub_matches
                .get_one::<String>("src_port")
                .unwrap()
                .to_string();

            do_udp_receiver(iface, src_port);
        }
        Some(("sender", sub_matches)) => {
            let iface = sub_matches.get_one::<String>("iface").unwrap().to_string();
            let dest_mac = sub_matches
                .get_one::<String>("dest_mac")
                .unwrap()
                .to_string();
            let dest_ip = sub_matches
                .get_one::<String>("dest_ip")
                .unwrap()
                .to_string();
            let dest_port = sub_matches
                .get_one::<String>("dest_port")
                .unwrap()
                .to_string();
            let payload = sub_matches
                .get_one::<String>("payload")
                .unwrap()
                .to_string();

            do_udp_sender(iface, dest_mac, dest_ip, dest_port, payload);
        }
        _ => todo!(),
    }
}

/****************************************************
 * UDP Receiver
 ****************************************************/
fn do_udp_receiver(iface_name: String, src_port: String) {
    let interface_name_match = |iface: &NetworkInterface| iface.name == iface_name;
    let interfaces = datalink::interfaces();
    let interface = interfaces.into_iter().find(interface_name_match).unwrap();

    let _src_mac_addr = interface.mac.unwrap();
    let _src_ip_addr = interface.ips[0].ip();
    let src_port: u16 = src_port.parse().unwrap();

    let mut nic = pv::Nic::new(
        &iface_name,
        CHUNK_SIZE,
        CHUNK_COUNT,
        FILL_RING_SIZE,
        COMP_RING_SIZE,
        TX_RING_SIZE,
        RX_RING_SIZE,
    )
    .unwrap_or_else(|err| panic!("Failed to create Nic: {}", err));

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

    unsafe {
        STATS.duration = Duration::from_secs(100000).as_secs() as usize;
        STATS.pkt_count = 0;
        STATS.total_bytes = 0;
        STATS_RUNNING = true;
    }
    thread::spawn(stats_thread);

    let rx_batch_size = 64;
    while !term.load(Ordering::Relaxed) {
        //thread::sleep(Duration::from_millis(500));

        let mut packets = nic.receive(rx_batch_size);
        let received = packets.len();

        if received > 0 {
            for packet in packets.iter_mut() {
                let packet_buffer = packet.get_buffer_mut();

                let eth_pkt = EthernetPacket::new(packet_buffer).expect("EthernetPacket Error");
                if eth_pkt.get_ethertype() != EtherType(0x0800) {
                    continue;
                }

                let ip_pkt = Ipv4Packet::new(eth_pkt.payload()).expect("Ipv4Packet Error");
                if ip_pkt.get_next_level_protocol() != IpNextHeaderProtocols::Udp {
                    continue;
                }

                let udp_pkt = UdpPacket::new(ip_pkt.payload()).expect("UdpPacket Error");
                if src_port != udp_pkt.get_destination() {
                    continue;
                }

                let packet_len = packet.end - packet.start;
                unsafe {
                    STATS.pkt_count += 1;
                    STATS.total_bytes += packet_len;
                }
            }
        }
    }
}

fn stats_thread() {
    let stats = unsafe { &mut STATS };
    let mut last_bytes = 0;
    let mut last_packets = 0;
    let start_time = Instant::now();
    let mut last_time = start_time;

    const SECOND: Duration = Duration::from_secs(1);

    while unsafe { STATS_RUNNING } {
        let elapsed = last_time.elapsed();

        if elapsed < SECOND {
            thread::sleep(SECOND - elapsed);
        }

        last_time = Instant::now();

        let bytes = stats.total_bytes;
        let bits = (bytes - last_bytes) * 8;
        let total_packets = stats.pkt_count;
        let packets = total_packets - last_packets;

        let lap = start_time.elapsed().as_secs();

        if lap > stats.duration as u64 {
            unsafe {
                STATS_RUNNING = false;
            }
            break;
        }

        println!(
            "{0}s: \
            {1} pps {2} bps",
            lap,
            //packets.to_formatted_string(&Locale::en),
            packets,
            //bits.to_formatted_string(&Locale::en),
            bits,
        );

        last_bytes = bytes;
        last_packets = total_packets;
    }
}

/****************************************************
 * UDP Sender
 ****************************************************/
fn do_udp_sender(
    iface_name: String,
    dest_mac: String,
    dest_ip: String,
    dest_port: String,
    payload: String,
) {
    let interface_name_match = |iface: &NetworkInterface| iface.name == iface_name;
    let interfaces = datalink::interfaces();
    let interface = interfaces.into_iter().find(interface_name_match).unwrap();

    let src_mac_addr: MacAddr = interface.mac.unwrap();
    let src_ip_addr: Ipv4Addr = match interface.ips[0].ip() {
        IpAddr::V4(ip4) => ip4,
        IpAddr::V6(_) => todo!(),
    };
    let src_port: u16 = dest_port.parse().unwrap();
    let src_port = src_port + 1;

    let dest_mac_addr: MacAddr = dest_mac.parse().unwrap();
    let dest_ip_addr: Ipv4Addr = dest_ip.parse().unwrap();
    let dest_port: u16 = dest_port.parse().unwrap();

    let payload_size: usize = payload.parse().unwrap();
    let udp_size: usize = 8;
    let ip_size: usize = 20;
    let eth_size: usize = 14;
    let packet_size: usize = eth_size + ip_size + udp_size + payload_size;

    let mut nic = pv::Nic::new(
        &iface_name,
        CHUNK_SIZE,
        CHUNK_COUNT,
        FILL_RING_SIZE,
        COMP_RING_SIZE,
        TX_RING_SIZE,
        RX_RING_SIZE,
    )
    .unwrap_or_else(|err| panic!("Failed to create Nic: {}", err));

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

    while !term.load(Ordering::Relaxed) {
        //thread::sleep(Duration::from_millis(500));

        let tx_batch_size = 1;
        let mut packets = Vec::<pv::Packet>::with_capacity(tx_batch_size);

        for _ in 0..tx_batch_size {
            let mut packet: pv::Packet = match nic.alloc_packet() {
                Some(packet) => packet,
                None => panic!("Packet allocation failed.."),
            };

            let mut buffer = vec![0u8; packet_size];

            let mut eth_pkt =
                MutableEthernetPacket::new(&mut buffer).expect("MutableEthernetPacket Error");
            eth_pkt.set_destination(src_mac_addr);
            eth_pkt.set_source(dest_mac_addr);
            eth_pkt.set_ethertype(EtherTypes::Ipv4);

            let mut ip_pkt =
                MutableIpv4Packet::new(eth_pkt.payload_mut()).expect("MutableIpv4Packet Error");
            ip_pkt.set_version(0x04);
            ip_pkt.set_header_length(0x05);
            ip_pkt.set_identification(0.try_into().unwrap());
            ip_pkt.set_total_length((packet_size - eth_size).try_into().unwrap());
            ip_pkt.set_ttl(0x40);
            ip_pkt.set_next_level_protocol(IpNextHeaderProtocols::Udp);
            ip_pkt.set_source(src_ip_addr);
            ip_pkt.set_destination(dest_ip_addr);
            ip_pkt.set_checksum(ipv4::checksum(&ip_pkt.to_immutable()));

            let mut udp_pkt =
                MutableUdpPacket::new(ip_pkt.payload_mut()).expect("MutableUdpPacket Error");
            udp_pkt.set_source(src_port);
            udp_pkt.set_destination(dest_port);
            udp_pkt.set_length((packet_size - eth_size - ip_size).try_into().unwrap());

            packet
                .replace_data(&buffer)
                .expect("Failed replace to payload of packet.");
            packets.push(packet);
        }

        let _ = nic.send(&mut packets);
    }
}
