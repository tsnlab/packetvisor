use clap::{arg, Command};
use pnet::{
    packet::{
        arp::MutableArpPacket,
        ethernet::{EtherTypes, MutableEthernetPacket},
        ip::IpNextHeaderProtocols,
        ipv4::MutableIpv4Packet,
        udp::{MutableUdpPacket, UdpPacket},
        MutablePacket,
    },
    util::MacAddr,
};
use std::{io, str::FromStr, thread, time::Duration};

use std::net::UdpSocket;

fn main() {
    let matches = Command::new("tunnel")
        .arg(arg!(gateway: -g --gateway <gateway> "Gateway interface").required(true))
        .arg(arg!(tunnel: -t --tunnel <tunnel> "Tunnel interface").required(true))
        .arg(
            arg!(destination: -d --destination <destination> "Destination ip address")
                .required(true),
        )
        .get_matches();

    let gateway = matches.get_one::<String>("gateway").unwrap();
    let tunnel = matches.get_one::<String>("tunnel").unwrap();
    let destination = matches.get_one::<String>("tunnel").unwrap();

    const CHUNK_SIZE: usize = 2048;
    const CHUNK_COUNT: usize = 1024;
    const RX_RING_SIZE: u32 = 64;
    const TX_RING_SIZE: u32 = 64;
    const FILLING_RING_SIZE: u32 = 64;
    const COMPLETION_RING_SIZE: u32 = 64;

    let mut gateway = pv::NIC::new(&gateway, CHUNK_SIZE, CHUNK_COUNT).unwrap();
    match gateway.open(
        RX_RING_SIZE,
        TX_RING_SIZE,
        FILLING_RING_SIZE,
        COMPLETION_RING_SIZE,
    ) {
        Ok(_) => {}
        Err(error) => println!("{error}"),
    }
    let mut tunnel = pv::NIC::new(&tunnel, CHUNK_SIZE, CHUNK_COUNT).unwrap();
    match tunnel.open(
        RX_RING_SIZE,
        TX_RING_SIZE,
        FILLING_RING_SIZE,
        COMPLETION_RING_SIZE,
    ) {
        Ok(_) => {}
        Err(error) => println!("{error}"),
    }

    loop {
        to_outside(&mut gateway, &mut tunnel);
        to_inside(&mut tunnel, &mut gateway);
    }

}

fn to_outside(from: &mut pv::NIC, to: &mut pv::NIC) {
    const RX_BATCH_SIZE: u32 = 64;
    let mut packets1: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);
    let mut packets2: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);

    let received = from.receive(&mut packets1);
    println!("received:{received}");

    if received > 0 {
        for packet in &mut packets1 {
            packet.dump();
        }
    } else {
        thread::sleep(Duration::from_millis(100));
    }
}

fn to_inside(from: &mut pv::NIC, to: &mut pv::NIC) {

}




fn forward(from: &mut pv::NIC, to: &mut pv::NIC) {
    const RX_BATCH_SIZE: u32 = 64;
    let mut packets1: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);
    let mut packets2: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);

    let received = from.receive(&mut packets1);

    if received > 0 {
        for packet in &mut packets1 {
            match process_packet(from, packet) {
                true => {
                    packets2.push(to.copy_from(packet).unwrap());
                }
                false => {}
            }
        }
        to.send(&mut packets2);
    } else {
        thread::sleep(Duration::from_millis(100));
    }
}

fn process_packet(nic: &mut pv::NIC, packet: &mut pv::Packet) -> bool {
    let buffer = packet.get_buffer_mut();

    let mut eth = MutableEthernetPacket::new(buffer).unwrap();
    match eth.get_ethertype() {
        EtherTypes::Ipv4 => {
            let mut ipv4 = MutableIpv4Packet::new(eth.payload_mut()).unwrap();
            match ipv4.get_next_level_protocol() {
                IpNextHeaderProtocols::Udp => {
                    let mut udp = MutableUdpPacket::new(ipv4.payload_mut()).unwrap();
                    let payload = udp.payload_mut();
                    let payload_len = payload.len();
                    let payload_vec = payload.to_vec();
                    packet.resize(payload_len);
                    packet.replace_data(&payload_vec).unwrap();

                    const RX_BATCH_SIZE: u32 = 64;
                    let mut packets: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);
                    packets.push(nic.copy_from(packet).unwrap());
                    nic.send(&mut packets);

                    false
                }
                _ => true,
            }
        }
        _ => true,
    }
}
