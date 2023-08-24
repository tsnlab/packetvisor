use clap::{arg, value_parser, Command};

use pnet::{
    packet::ipv4::MutableIpv4Packet,
    packet::MutablePacket,
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        udp::{self, MutableUdpPacket},
    },
};
use signal_hook::SigId;
use std::{
    io::Error,
    net::Ipv4Addr,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let matches = Command::new("filter")
        .arg(arg!(nic1: --nic1 <nic1> "nic1 to use").required(true))
        .arg(arg!(nic2: -p --nic2 <nic2> "nic2 to use").required(true))
        .arg(arg!(source: --"source-word" <source_word> "source word to be changed").required(true))
        .arg(arg!(target: --"target-word" <target_word> "target word to change").required(true))
        .arg(
            arg!(chunk_size: -s --"chunk-size" <size> "Chunk size")
                .value_parser(value_parser!(usize))
                .required(false)
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
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .value_parser(value_parser!(u32))
                .required(false)
                .default_value("64"),
        )
        .get_matches();

    let name1 = matches.get_one::<String>("nic1").unwrap().clone();
    let name2 = matches.get_one::<String>("nic2").unwrap().clone();
    let chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    let chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    let rx_ring_size = *matches.get_one::<u32>("rx_ring_size").unwrap();
    let tx_ring_size = *matches.get_one::<u32>("tx_ring_size").unwrap();
    let filling_ring_size = *matches.get_one::<u32>("fill_ring_size").unwrap();
    let completion_ring_size = *matches.get_one::<u32>("completion_ring_size").unwrap();
    let source_word: String = matches.get_one::<String>("source").unwrap().clone();
    let target_word: String = matches.get_one::<String>("target").unwrap().clone();

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

    let mut nic1 = pv::NIC::new(&name1, chunk_size, chunk_count).unwrap();
    nic1.open(
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    )
    .unwrap();
    let mut nic2 = pv::NIC::new(&name2, chunk_size, chunk_count).unwrap();
    nic2.open(
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    )
    .unwrap();

    while !term.load(Ordering::Relaxed) {
        forward(&mut nic1, &mut nic2, &source_word, &target_word);
        forward(&mut nic2, &mut nic1, &source_word, &target_word);
    }
}

fn forward(from: &mut pv::NIC, to: &mut pv::NIC, source_word: &String, target_word: &String) {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets1: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);
    let mut packets2: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);
    let received = from.receive(&mut packets1);

    if received > 0 {
        for packet in &mut packets1 {
            match process_packet(packet, source_word, target_word) {
                true => {
                    packets2.push(to.copy_from(packet).unwrap());
                }
                false => {}
            }
        }

        for retry in (0..3).rev() {
            match (to.send(&mut packets2), retry) {
                (cnt, _) if cnt > 0 => break, // Success
                (0, 0) => {
                    // Failed 3 times
                    for packet in packets2 {
                        to.free(&packet);
                    }
                    break;
                }
                _ => continue, // Retrying
            }
        }
    } else {
        // No packets received. Sleep
        thread::sleep(Duration::from_millis(100));
    }
}

fn process_packet(packet: &mut pv::Packet, source_word: &String, target_word: &String) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut ipv4: MutableIpv4Packet<'_>;
    let mut udp: MutableUdpPacket<'_>;
    let source_addr: Ipv4Addr;
    let destination_addr: Ipv4Addr;

    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => return true,
    };

    match eth.get_ethertype() {
        EtherTypes::Ipv4 => {
            ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
        }
        _ => return true,
    }

    match ipv4.get_next_level_protocol() {
        IpNextHeaderProtocols::Udp => {
            source_addr = ipv4.get_source();
            destination_addr = ipv4.get_destination();
            udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();
        }
        _ => return true,
    }

    let udp_payload: &mut [u8] = udp.payload_mut();
    let message = String::from_utf8_lossy(&udp_payload);
    let new_message = message.replace(source_word, target_word);
    udp.set_payload(new_message.as_bytes());
    udp.set_checksum(udp::ipv4_checksum(
        &udp.to_immutable(),
        &source_addr,
        &destination_addr,
    ));

    true
}
