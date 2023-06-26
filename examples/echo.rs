use packetvisor::pv;

use clap::{arg, Command};

use pnet::{
    datalink::{interfaces, MacAddr, NetworkInterface},
    packet::ethernet::{EtherTypes, MutableEthernetPacket},
    packet::MutablePacket,
    packet::{
        arp::{ArpOperations, MutableArpPacket},
        PacketSize,
    },
};
use signal_hook::SigId;
use std::{
    env,
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
    thread,
    time::Duration,
};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 8 {
        println!("check the number of arguments.");
        std::process::exit(-1);
    }

    let matches = Command::new("echo")
        .arg(arg!(interface: -i --interface <interface> "Interface to use").required(true))
        .arg(arg!(chunk_size: -s --chunk-size <size> "Chunk size").required(true).default_value("2048"))
        .arg(arg!(chunk_count: -c --chunk-count <count> "Chunk count").required(true).default_value("1024"))
        .arg(arg!(rx_ring_size: -r --rx-ring <count> "Rx ring size").required(true).default_value("64"))
        .arg(arg!(tx_ring_size: -t --tx-ring <count> "Tx ring size").required(true).default_value("64"))
        .arg(arg!(fill_ring_size: -f --fill-ring <count> "Fill ring size").required(true).default_value("64"))
        .arg(arg!(completion_ring_size: -o --completion-ring <count> "Completion ring size").required(true).default_value("64"))
        .get_matches();

    let if_name = matches.get_one::<String>("interface").unwrap().clone();
    let chunk_size: u32 = matches.get_one::<String>("chunk_size").unwrap().parse().unwrap();
    let chunk_count = matches.get_one::<String>("chunk_count").unwrap().parse().unwrap();
    let rx_ring_size = matches.get_one::<String>("rx_ring_size").unwrap().parse().unwrap();
    let tx_ring_size = matches.get_one::<String>("tx_ring_size").unwrap().parse().unwrap();
    let filling_ring_size = matches.get_one::<String>("fill_ring_size").unwrap().parse().unwrap();
    let completion_ring_size = matches.get_one::<String>("completion_ring_size").unwrap().parse().unwrap();


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
    let interface = all_interfaces
        .iter()
        .find(|element| element.name.as_str() == if_name.as_str());

    let src_mac_address: MacAddr = match interface {
        Some(target_interface) => target_interface.mac.unwrap(),
        None => panic!("couldn't find source MAC address"),
    };

    let mut nic = match pv::pv_open(&if_name, chunk_size, chunk_count, rx_ring_size, tx_ring_size, filling_ring_size, completion_ring_size) {
        Some(nic) => nic,
        None => panic!("nic allocation failed!"),
    };

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received = pv::pv_receive(&mut nic, &mut packets, rx_batch_size);

        if received == 0 {
            // No packets received. Sleep
            thread::sleep(Duration::from_millis(1));
        }

        for packet in packets.iter() {
            // FIXME: implement to_owned()
            if process_packet(&mut nic, *packet, &src_mac_address).is_err() {
                println!("Failed to process packet");
            }
        }
    }

    pv::pv_close(nic);
    println!("PV END");
}

fn process_packet(
    nic: &mut pv::Nic,
    mut packet: pv::Packet,
    src_mac: &MacAddr,
) -> Result<(), String> {
    let mut buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(&mut buffer).unwrap();

    // Swap source and destination
    eth.set_destination(eth.get_source());
    eth.set_source(*src_mac);

    let to_send: bool = match eth.get_ethertype() {
        EtherTypes::Arp => {
            process_arp(&mut packet, src_mac)
        },
        EtherTypes::Ipv4 => {
            // TODO
            false
        }
        _ => {
            return Err(format!(
                "Unsupported ethernet protocol {:?}",
                eth.get_ethertype()
            ));
        }
    };

    if to_send {
        pv::pv_send(nic, &mut Vec::from([packet]), 1);
    } else {
        pv::pv_free(nic, &mut Vec::from([packet]), 0);
    }

    Ok(())
}

fn process_arp(packet: &mut pv::Packet, src_mac: &MacAddr) -> bool {
    let mut buffer = packet.get_buffer_mut();
    let mut eth = MutableEthernetPacket::new(&mut buffer).unwrap();
    let mut arp = MutableArpPacket::new(eth.payload_mut()).unwrap();

    if arp.get_operation() != ArpOperations::Request {
        return false;
    }

    let target_ip = arp.get_target_proto_addr();

    arp.set_operation(ArpOperations::Reply);
    arp.set_target_hw_addr(arp.get_sender_hw_addr());
    arp.set_target_proto_addr(arp.get_target_proto_addr());
    arp.set_sender_hw_addr(*src_mac);
    arp.set_sender_proto_addr(target_ip);

    // Adjust packet buffer
    let packet_size = arp.packet_size() + eth.packet_size();
    packet.end = packet.start + packet_size as u32;

    true
}
