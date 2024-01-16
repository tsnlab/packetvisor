use clap::{arg, Command};

use signal_hook::SigId;
use std::{
    io::Error,
    net::{IpAddr, Ipv4Addr},
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::{Duration, SystemTime},
};

use num_derive::FromPrimitive;
use num_traits::FromPrimitive;

use pnet_macros::packet;
use pnet_macros_support::types::u32be;
use pnet_packet::{MutablePacket, Packet};

use pnet::{
    datalink,
    datalink::{MacAddr, NetworkInterface},
    packet::{
        ethernet::{EtherTypes, EthernetPacket, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        ipv4,
        ipv4::{Ipv4Packet, MutableIpv4Packet},
        udp::{MutableUdpPacket, UdpPacket},
    },
};

/****************************************************
 * Perf Packet Structure
 ****************************************************/
#[packet]
pub struct Perf {
    id: u32be,
    op: u8,
    #[payload]
    payload: Vec<u8>,
}

#[derive(FromPrimitive)]
enum PerfOp {
    /* Ping, Pong for RTT mode */
    Ping = 0,
    Pong = 1,

    SYN = 99,
    ACK = 100,
}

/****************************************************
 * for XSK and UMEM
 ****************************************************/
const CHUNK_SIZE: usize = 2048;
const CHUNK_COUNT: usize = 1024;
const FILL_RING_SIZE: usize = 64;
const COMP_RING_SIZE: usize = 64;
const TX_RING_SIZE: usize = 64;
const RX_RING_SIZE: usize = 64;
const UMEM_SHARE: bool = false;
const TX_BATCH_SIZE: usize = 1;
const RX_BATCH_SIZE: usize = 1;

/****************************************************
 * Main
 ****************************************************/
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
        )
        .arg(
            arg!(interval: -d <delay_interval> "Latency Measurement Interval(sec) - default 1s")
                .required(false)
                .default_value("1"),
        )
        .arg(
            arg!(count: -c <count> "Measurement Count - default 10")
                .required(false)
                .default_value("10"),
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
            let interval = sub_matches
                .get_one::<String>("interval")
                .unwrap()
                .to_string();
            let count = sub_matches.get_one::<String>("count").unwrap().to_string();

            do_udp_sender(
                iface, dest_mac, dest_ip, dest_port, payload, interval, count,
            );
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

    let src_mac_addr = interface.mac.unwrap();
    let src_ip_addr = interface.ips[0].ip();
    let src_port: u16 = src_port.parse().unwrap();

    let mut pool = match pv::Pool::new(
        CHUNK_SIZE,
        CHUNK_COUNT,
        FILL_RING_SIZE,
        COMP_RING_SIZE,
        UMEM_SHARE,
    ) {
        Ok(pool) => {
            println!("Pool created");
            pool
        }
        Err(err) => {
            panic!("Failed to create buffer pool: {}", err);
        }
    };

    let mut nic = match pv::Nic::new(&iface_name, &mut pool, TX_RING_SIZE, RX_RING_SIZE) {
        Ok(nic) => {
            println!("Nic created");
            nic
        }
        Err(err) => {
            panic!("Failed to create NIC : {}", err);
        }
    };

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

    let mut drop = false;

    while !term.load(Ordering::Relaxed) {
        let mut packets = nic.receive(RX_BATCH_SIZE);
        let received = packets.len();

        if received > 0 {
            /* Rx */
            for packet in packets.iter_mut() {
                let packet_buffer = packet.get_buffer_mut();

                // Check L2
                let mut eth_pkt =
                    MutableEthernetPacket::new(packet_buffer).expect("MutableEthernetPacket Error");

                if src_mac_addr != eth_pkt.get_destination() {
                    continue;
                }

                // Check L3
                let mut ip_pkt =
                    MutableIpv4Packet::new(eth_pkt.payload_mut()).expect("MutableIpv4Packet Error");

                if src_ip_addr != ip_pkt.get_destination() {
                    continue;
                }

                // Check L4
                let mut udp_pkt =
                    MutableUdpPacket::new(ip_pkt.payload_mut()).expect("MutableUdpPacket Error");

                if src_port != udp_pkt.get_destination() {
                    continue;
                }

                let mut perf_pkt =
                    MutablePerfPacket::new(udp_pkt.payload_mut()).expect("MutablePerfPacket Error");

                match PerfOp::from_u8(perf_pkt.get_op()) {
                    Some(PerfOp::SYN) => {
                        perf_pkt.set_op(PerfOp::ACK as u8);
                    }
                    Some(PerfOp::Ping) => {
                        perf_pkt.set_op(PerfOp::Pong as u8);
                    }
                    _ => {
                        drop = true;
                    }
                }
            }

            /* Tx */
            if !drop {
                let _ = nic.send(&mut packets);
            }
            drop = false;
        }
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
    interval: String,
    count: String,
) {
    #[derive(PartialEq, Clone)]
    enum SenderState {
        Ready = 0,
        Running = 1,
    }
    let mut state = SenderState::Ready;

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

    let interval: f32 = interval.parse().unwrap();
    let interval: usize = (interval * 1000.0).round() as usize;

    let count: usize = count.parse().unwrap();
    let mut pkt_idx: usize = 1; // 1 to count

    let mut pool = match pv::Pool::new(
        CHUNK_SIZE,
        CHUNK_COUNT,
        FILL_RING_SIZE,
        COMP_RING_SIZE,
        UMEM_SHARE,
    ) {
        Ok(pool) => {
            println!("Pool created");
            pool
        }
        Err(err) => {
            panic!("Failed to create buffer pool: {}", err);
        }
    };

    let mut nic = match pv::Nic::new(&iface_name, &mut pool, TX_RING_SIZE, RX_RING_SIZE) {
        Ok(nic) => {
            println!("Nic created");
            nic
        }
        Err(err) => {
            panic!("Failed to create NIC : {}", err);
        }
    };

    /* Session Init & RTT(UDP Ping-Pong) */
    loop {
        match state {
            SenderState::Running => {
                if pkt_idx > count {
                    break;
                }

                thread::sleep(Duration::from_millis(interval.try_into().unwrap()));
            }
            _ => {}
        }

        /* Tx */
        let mut packets = Vec::<pv::Packet>::with_capacity(TX_BATCH_SIZE);
        let mut packet: pv::Packet = match pool.try_alloc_packet() {
            Some(packet) => packet,
            None => panic!("Packet allocation failed.."),
        };

        let mut buffer = vec![0u8; packet_size];

        let mut eth_pkt =
            MutableEthernetPacket::new(&mut buffer).expect("MutableEthernetPacket Error");
        eth_pkt.set_source(src_mac_addr);
        eth_pkt.set_destination(dest_mac_addr);
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

        let mut perf_pkt =
            MutablePerfPacket::new(udp_pkt.payload_mut()).expect("MutablePerfPacket Error");

        match state {
            SenderState::Ready => {
                perf_pkt.set_id(0 as u32);
                perf_pkt.set_op(PerfOp::SYN as u8);
            }
            SenderState::Running => {
                perf_pkt.set_id(pkt_idx as u32);
                perf_pkt.set_op(PerfOp::Ping as u8);
            }
        }

        packet
            .replace_data(&buffer)
            .expect("Failed replace to payload of packet.");
        packets.push(packet);

        // Get Tx Timestamp
        let tx_timestamp = SystemTime::now();
        let _ = nic.send(&mut packets);

        let mut ping_pong_ok = false;
        let pre_state = state.clone();

        /* Rx */
        loop {
            match state {
                SenderState::Ready => {
                    thread::sleep(Duration::from_millis(100));
                }
                SenderState::Running => {
                    if (pre_state != state) || (ping_pong_ok) {
                        break;
                    }
                }
            }

            let mut packets = nic.receive(RX_BATCH_SIZE);
            let received = packets.len();

            if received > 0 {
                // Get Rx Timestamp
                let rx_timestamp = SystemTime::now();

                for packet in packets.iter_mut() {
                    let packet_buffer = packet.get_buffer_mut();

                    let eth_pkt = EthernetPacket::new(packet_buffer).expect("EthernetPacket Error");
                    let ip_pkt = Ipv4Packet::new(eth_pkt.payload()).expect("Ipv4Packet Error");
                    let udp_pkt = UdpPacket::new(ip_pkt.payload()).expect("UdpPacket Error");
                    let perf_pkt = PerfPacket::new(udp_pkt.payload()).expect("PerfPacket Error");

                    match PerfOp::from_u8(perf_pkt.get_op()) {
                        Some(PerfOp::ACK) => {
                            state = SenderState::Running;
                            break;
                        }
                        Some(PerfOp::Pong) => {
                            if pkt_idx != perf_pkt.get_id().try_into().unwrap() {
                                println!("Not Matched");
                                continue;
                            }

                            // Calculate RTT
                            let rtt = rx_timestamp.duration_since(tx_timestamp).unwrap();
                            println!(
                                "pkt id[{}]: RTT {}.{:09}s",
                                pkt_idx,
                                rtt.as_secs(),
                                rtt.subsec_nanos()
                            );

                            ping_pong_ok = true;
                            pkt_idx += 1;
                            break;
                        }
                        _ => {
                            println!("ERROR");
                            break;
                        }
                    }
                }
            }
        }
    }
}
