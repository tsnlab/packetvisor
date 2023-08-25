use clap::{arg, value_parser, Command};

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
            match check_udp(packet) {
                true => change_word(packet, source_word, target_word),
                false => {}
            }
            packets2.push(to.copy_from(packet).unwrap());
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


// check if packet is udp or not
fn check_udp(packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();
    let mut eth = match MutableEthernetPacket::new(buffer) {
        Some(eth) => eth,
        None => return false,
    };

    match eth.get_ethertype() {
        EtherTypes::Ipv4 => {
            let ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
            match ipv4.get_next_level_protocol() {
                IpNextHeaderProtocols::Udp => return true,
                _ => return false,
            }
        }
        _ => return false,
    }
}

fn change_word(packet: &mut pv::Packet, source_word: &String, target_word: &String) {
    let mut eth = MutableEthernetPacket::new(packet.get_buffer_mut()).unwrap();
    let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();

    // export payload from packet & change source_words to target_words
    let payload = udp.payload_mut();
    let payload_data = String::from_utf8_lossy(&payload);
    let new_payload_data = payload_data.replace(source_word, target_word);
    let new_payload = new_payload_data.as_bytes();

    // create new udp vector & set checksum
    let mut new_udp_vec = create_new_udp(&udp, new_payload);
    let mut new_udp = MutableUdpPacket::new(new_udp_vec.as_mut_slice()).unwrap();
    new_udp.set_checksum(udp::ipv4_checksum(
        &new_udp.to_immutable(),
        &ipv4.get_source(),
        &ipv4.get_destination(),
    ));

    // create new ipv4 & set checksum
    let mut new_ipv4_vec = create_new_ipv4(&ipv4, new_udp_vec.as_mut_slice());
    let mut new_ipv4 = MutableIpv4Packet::new(new_ipv4_vec.as_mut_slice()).unwrap();
    new_ipv4.set_checksum(ipv4::checksum(&new_ipv4.to_immutable()));

    // create new eth & replace buffer
    let new_eth = create_new_eth(&eth, new_ipv4_vec.as_mut_slice());
    let _ = packet.replace_data(&new_eth);
}

fn create_new_udp(udp: &MutableUdpPacket<'_>, payload: &[u8]) -> Vec<u8> {
    let mut new_udp: Vec<u8> = Vec::new();

    // copy source port
    let slice = udp.get_source().to_be_bytes();
    new_udp.push(slice[0]);
    new_udp.push(slice[1]);

    // copy destination port
    let slice = udp.get_destination().to_be_bytes();
    new_udp.push(slice[0]);
    new_udp.push(slice[1]);

    // set udp length
    let slice = (8 + payload.len() as u16).to_be_bytes();
    new_udp.push(slice[0]);
    new_udp.push(slice[1]);

    // arbitarily set checksum as 0
    new_udp.push(0);
    new_udp.push(0);

    // copy payload
    for p in payload.into_iter() {
        new_udp.push(*p);
    }

    new_udp
}
fn create_new_ipv4(ipv4: &MutableIpv4Packet<'_>, payload: &[u8]) -> Vec<u8> {
    let mut new_ipv4: Vec<u8> = Vec::new();

    // copy version & header length
    let version = ipv4.get_version() * 16;
    let header_length = ipv4.get_header_length();
    new_ipv4.push(version + header_length);

    // copy dscn & ecn
    let dscp = ipv4.get_dscp() * 16;
    let ecn = ipv4.get_ecn();
    new_ipv4.push(dscp + ecn);

    // set total length
    let slice = (20 + payload.len() as u16).to_be_bytes();
    new_ipv4.push(slice[0]);
    new_ipv4.push(slice[1]);

    // copy identification
    let identification = ipv4.get_identification();
    let slice = identification.to_be_bytes();
    new_ipv4.push(slice[0]);
    new_ipv4.push(slice[1]);

    // copy flags & fragment
    let flags: u16 = (ipv4.get_flags() as u16) * 32 * 256;
    let fragment_offset = ipv4.get_fragment_offset();
    let slice = (flags + fragment_offset).to_be_bytes();
    new_ipv4.push(slice[0]);
    new_ipv4.push(slice[1]);

    // copy ttl
    let ttl = ipv4.get_ttl();
    new_ipv4.push(ttl);

    // copy protocol
    let protocol = ipv4.get_next_level_protocol().0;
    new_ipv4.push(protocol);

    // arbitarily set checksum as 0
    new_ipv4.push(0);
    new_ipv4.push(0);

    // copy source ip address
    let source_addr = ipv4.get_source().octets();
    new_ipv4.push(source_addr[0]);
    new_ipv4.push(source_addr[1]);
    new_ipv4.push(source_addr[2]);
    new_ipv4.push(source_addr[3]);

    // copy destination ip address
    let destination_addr = ipv4.get_destination().octets();
    new_ipv4.push(destination_addr[0]);
    new_ipv4.push(destination_addr[1]);
    new_ipv4.push(destination_addr[2]);
    new_ipv4.push(destination_addr[3]);

    // copy payload
    for p in payload.into_iter() {
        new_ipv4.push(*p);
    }

    new_ipv4
}
fn create_new_eth(eth: &MutableEthernetPacket<'_>, payload: &[u8]) -> Vec<u8> {
    let mut new_eth: Vec<u8> = Vec::new();

    // copy destination MAC address
    let destination = eth.get_destination().octets();
    for d in destination.into_iter() {
        new_eth.push(d);
    }

    // copy source MAC address
    let source = eth.get_source().octets();
    for s in source.into_iter() {
        new_eth.push(s);
    }

    // copy protocol
    let protocol = eth.get_ethertype().0;
    let slice = protocol.to_be_bytes();
    new_eth.push(slice[0]);
    new_eth.push(slice[1]);

    // copy payload
    for p in payload.into_iter() {
        new_eth.push(*p);
    }

    new_eth
}
