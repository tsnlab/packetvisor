use clap::{arg, Command};

use signal_hook::SigId;
use std::{
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::{Duration, Instant},
    net::{IpAddr, Ipv4Addr},
};

use pnet::{
    datalink,
    datalink::{MacAddr, NetworkInterface},
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        ipv4,
        ipv4::MutableIpv4Packet,
        udp::MutableUdpPacket,
        MutablePacket,
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
const UMEM_SHARE: bool = false;

fn main() {
    let receiver_command = Command::new("receiver")
        .about("Receiver mode")
        .arg(arg!(iface: -i <iface> "Interface to use").required(true))
        .arg(arg!(src_port: <port> "Source Port Number").required(true));

    let sender_command = Command::new("sender")
        .about("Sender mode")
        .arg(arg!(iface: -i <iface> "Interface to use").required(true))
        .arg(arg!(dest_mac: <mac> "Destination MAC Address").required(true))
        .arg(arg!(dest_ip: <ip> "Destination IP Address").required(true))
        .arg(arg!(dest_port: <port> "Destination Port Number").required(true));

    let matched_command = Command::new("throughput")
        .subcommand_required(true)
        .arg_required_else_help(true)
        .subcommand(receiver_command)
        .subcommand(sender_command)
        .get_matches();

    match matched_command.subcommand() {
        Some(("receiver", sub_matches)) => {
            let iface = sub_matches.get_one::<String>("iface").unwrap().to_string();
            let src_port = sub_matches.get_one::<String>("src_port").unwrap().to_string();

            do_udp_receiver(iface, src_port);
        }
        Some(("sender", sub_matches)) => {
            let iface = sub_matches.get_one::<String>("iface").unwrap().to_string();
            let dest_mac = sub_matches.get_one::<String>("dest_mac").unwrap().to_string();
            let dest_ip = sub_matches.get_one::<String>("dest_ip").unwrap().to_string();
            let dest_port = sub_matches.get_one::<String>("dest_port").unwrap().to_string();

            do_udp_sender(iface, dest_mac, dest_ip, dest_port);
        },
        _ => todo!()
    }
}

/****************************************************
 * UDP Receiver
 ****************************************************/
fn do_udp_receiver(iface_name: String, src_port: String) {
    let interface_name_match = |iface: &NetworkInterface| iface.name == iface_name;
    let interfaces = datalink::interfaces();
    let interface = interfaces.into_iter().find(interface_name_match).unwrap();

    let my_mac_addr = interface.mac.unwrap();
    let my_ip_addr = interface.ips[0].ip();
    let my_src_port: u16 = src_port.parse().unwrap();

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
        },
        Err(err) => {
            panic!("Failed to create buffer pool: {}", err);
        }
    };

    let mut nic = match pv::Nic::new(
        &iface_name,
        &mut pool,
        TX_RING_SIZE,
        RX_RING_SIZE,
    ) {
        Ok(nic) => {
            println!("Nic created");
            nic
        },
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
            unsafe { STATS.pkt_count += received; }

            for packet in packets.iter_mut() {
                let packet_len = packet.end-packet.start;
                unsafe { STATS.total_bytes += packet_len; }
            }

            /* for Debug
            for packet in packets.iter_mut() {

                println!("Packet Length : {}", packet.end - packet.start);
                packet.dump();
            }
            */
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
fn do_udp_sender(iface_name: String, dest_mac: String, dest_ip: String, dest_port: String) {
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
        },
        Err(err) => {
            panic!("Failed to create buffer pool: {}", err);
        }
    };

    let mut nic = match pv::Nic::new(
        &iface_name,
        &mut pool,
        TX_RING_SIZE,
        RX_RING_SIZE,
    ) {
        Ok(nic) => {
            println!("Nic created");
            nic
        },
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

    while !term.load(Ordering::Relaxed) {
        //thread::sleep(Duration::from_millis(500));

        let tx_batch_size = 1;
        let mut packets = Vec::<pv::Packet>::with_capacity(tx_batch_size);

        for _ in 0..tx_batch_size {
            let mut packet: pv::Packet = match pool.try_alloc_packet() {
                Some(packet) => packet,
                None => panic!("Packet allocation failed.."),
            };

            let mut buffer = vec![0u8; 1440];

            let mut eth_pkt = MutableEthernetPacket::new(&mut buffer).expect("MutableEthernetPacket Error");
            eth_pkt.set_destination(src_mac_addr);
            eth_pkt.set_source(dest_mac_addr);
            eth_pkt.set_ethertype(EtherTypes::Ipv4);

            let mut ip_pkt = MutableIpv4Packet::new(eth_pkt.payload_mut()).expect("MutableIpv4Packet Error");
            ip_pkt.set_version(0x04);
            ip_pkt.set_header_length(0x05);
            ip_pkt.set_identification(0.try_into().unwrap());
            ip_pkt.set_total_length((1440-14).try_into().unwrap());
            ip_pkt.set_ttl(0x40);
            ip_pkt.set_next_level_protocol(IpNextHeaderProtocols::Udp);
            ip_pkt.set_source(src_ip_addr);
            ip_pkt.set_destination(dest_ip_addr);
            ip_pkt.set_checksum(ipv4::checksum(&ip_pkt.to_immutable()));

            let mut udp_pkt = MutableUdpPacket::new(ip_pkt.payload_mut()).expect("MutableUdpPacket Error");
            udp_pkt.set_source(src_port);
            udp_pkt.set_destination(dest_port);
            udp_pkt.set_length((1440-14-20).try_into().unwrap());

            packet.replace_data(&buffer).expect("Failed replace to payload of packet.");
            packets.push(packet);
        }

        let _ = nic.send(&mut packets);
    }
}
