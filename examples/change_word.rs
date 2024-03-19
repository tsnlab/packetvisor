use clap::{arg, value_parser, ArgMatches, Command};

use pnet::{
    packet::ipv4::MutableIpv4Packet,
    packet::MutablePacket,
    packet::{
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        ipv4,
        udp::{self, MutableUdpPacket},
    },
};
use signal_hook::SigId;
use std::{
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let cli_options = parse_cli_options();

    let if_name1 = cli_options.get_one::<String>("nic1").unwrap().clone();
    let if_name2 = cli_options.get_one::<String>("nic2").unwrap().clone();
    let chunk_size = *cli_options.get_one::<usize>("chunk_size").unwrap();
    let chunk_count = *cli_options.get_one::<usize>("chunk_count").unwrap();
    let rx_ring_size = *cli_options.get_one::<usize>("rx_ring_size").unwrap();
    let tx_ring_size = *cli_options.get_one::<usize>("tx_ring_size").unwrap();
    let filling_ring_size = *cli_options.get_one::<usize>("fill_ring_size").unwrap();
    let completion_ring_size = *cli_options
        .get_one::<usize>("completion_ring_size")
        .unwrap();
    let source_word = cli_options.get_one::<String>("source").unwrap().clone();
    let change_word = cli_options.get_one::<String>("change").unwrap().clone();

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

    let mut nic1 = match pv::Nic::new(
        &if_name1,
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        tx_ring_size,
        rx_ring_size,
    ) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC1: {}", err);
        }
    };

    let mut nic2 = match pv::Nic::new(
        &if_name2,
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        tx_ring_size,
        rx_ring_size,
    ) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC2: {}", err);
        }
    };

    while !term.load(Ordering::Relaxed) {
        if let Some(sent_cnt) = forward(&mut nic1, &mut nic2, &source_word, &change_word) {
            println!(
                "[{} -> {}] Sent Packet Count : {}",
                nic1.interface.name, nic2.interface.name, sent_cnt
            );
        }

        if let Some(sent_cnt) = forward(&mut nic2, &mut nic1, &source_word, &change_word) {
            println!(
                "[{} -> {}] Sent Packet Count : {}",
                nic2.interface.name, nic1.interface.name, sent_cnt
            );
        }

        thread::sleep(Duration::from_millis(100));
    }
}

fn forward(
    from: &mut pv::Nic,
    to: &mut pv::Nic,
    source_word: &str,
    target_word: &str,
) -> Option<usize> {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: usize = 64;
    let mut packets = from.receive(rx_batch_size);
    let received = packets.len();

    if 0 == received {
        return None;
    }

    let mut change_word_packets: Vec<pv::Packet> = Vec::with_capacity(received);
    for packet in &mut packets {
        match is_udp(packet) {
            true => change_word(packet, source_word, target_word),
            false => {}
        }

        let packet_data = packet.get_buffer_mut().to_vec();
        let mut change_word_packet = to.alloc_packet().unwrap();
        change_word_packet.replace_data(&packet_data).unwrap();

        change_word_packets.push(change_word_packet);
    }

    for _ in 0..3 {
        let sent_cnt = to.send(&mut change_word_packets);

        if sent_cnt > 0 {
            return Some(sent_cnt);
        }
    }

    None
}

// check if packet is udp or not
fn is_udp(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => return false,
    };

    match eth.get_ethertype() {
        EtherTypes::Ipv4 => {
            let ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
            matches!(ipv4.get_next_level_protocol(), IpNextHeaderProtocols::Udp)
        }
        _ => false,
    }
}

// change all matched source_word to target_word
fn change_word(packet: &mut pv::Packet, source_word: &str, target_word: &str) {
    /*
      get difference of length between original payload and changed payload
      get original payload data also.
      because when extract payload from udp after change udp's length field,
      original payload could be lost.
    */
    let diff = get_diff(packet, source_word, target_word);
    let original_payload_data = get_original_payload_data(packet);

    if diff.is_negative() {
        packet
            .resize(packet.end - packet.start - diff.wrapping_abs() as usize)
            .unwrap();
    } else {
        packet
            .resize(packet.end - packet.start + diff as usize)
            .unwrap();
    }

    let mut eth = MutableEthernetPacket::new(packet.get_buffer_mut()).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let ipv4_source_addr = ipv4.get_source();
    let ipv4_destination_addr = ipv4.get_destination();

    // change ipv4 total length field
    ipv4.set_total_length((ipv4.get_total_length() as isize + diff) as u16);

    // change udp length field
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();
    udp.set_length((udp.get_length() as isize + diff) as u16);

    // change source_words to target_words
    let payload = udp.payload_mut();
    let new_payload_data = original_payload_data.replace(source_word, target_word);
    let new_payload = new_payload_data.as_bytes();
    if diff.is_negative() {
        payload.copy_from_slice(new_payload);
    } else {
        payload.copy_from_slice(&new_payload[0..payload.len()]);
    }

    // change udp checksum
    udp.set_checksum(udp::ipv4_checksum(
        &udp.to_immutable(),
        &ipv4_source_addr,
        &ipv4_destination_addr,
    ));

    ipv4.set_checksum(ipv4::checksum(&ipv4.to_immutable()));
}

fn get_diff(packet: &mut pv::Packet, source_word: &str, target_word: &str) -> isize {
    let payload_data: String;

    let mut eth = MutableEthernetPacket::new(packet.get_buffer_mut()).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();

    let payload = udp.payload_mut();
    unsafe {
        payload_data = String::from_utf8_unchecked(payload.to_vec());
    }
    let new_payload_data = payload_data.replace(source_word, target_word);
    new_payload_data.len() as isize - payload_data.len() as isize
}

fn get_original_payload_data(packet: &mut pv::Packet) -> String {
    let mut eth = MutableEthernetPacket::new(packet.get_buffer_mut()).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();
    let payload = udp.payload_mut();
    unsafe { String::from_utf8_unchecked(payload.to_vec()) }
}

fn parse_cli_options() -> ArgMatches {
    Command::new("change_word")
        .arg(arg!(nic1: --nic1 <nic1> "nic1 to use").required(true))
        .arg(arg!(nic2: -p --nic2 <nic2> "nic2 to use").required(true))
        .arg(arg!(source: --"source-word" <source_word> "source word to be changed").required(true))
        .arg(arg!(change: --"change-word" <change_word> "target word to change").required(true))
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
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(tx_ring_size: -t --"tx-ring" <count> "Tx ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(fill_ring_size: -f --"fill-ring" <count> "Fill ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .arg(
            arg!(completion_ring_size: -o --"completion-ring" <count> "Completion ring size")
                .value_parser(value_parser!(usize))
                .required(false)
                .default_value("64"),
        )
        .get_matches()
}
