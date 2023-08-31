use clap::{arg, Command};
use std::{net::UdpSocket, io};

fn main() {
    let matches = Command::new("tunnel")
        .arg(arg!(interface: -i --interface <interface> "Interface"))
        .arg(arg!(source: -s --source <source> "Source IP address"))
        .arg(
            arg!(destination: -d --destination <destination> "Destination IP address")
                .required(true),
        )
        .get_matches();

    let interface = matches.get_one::<String>("interface").unwrap();
    let source = matches.get_one::<String>("source").unwrap();
    let destination = matches.get_one::<String>("destination").unwrap();

    const CHUNK_SIZE: usize = 2048;
    const CHUNK_COUNT: usize = 1024;
    const RX_RING_SIZE: u32 = 64;
    const TX_RING_SIZE: u32 = 64;
    const FILLING_RING_SIZE: u32 = 64;
    const COMPLETION_RING_SIZE: u32 = 64;

    let mut interface = pv::NIC::new(&interface, CHUNK_SIZE, CHUNK_COUNT).unwrap();
    match interface.open(
        RX_RING_SIZE,
        TX_RING_SIZE,
        FILLING_RING_SIZE,
        COMPLETION_RING_SIZE,
    ) {
        Ok(_) => {}
        Err(error) => println!("{error}"),
    }
    let socket = UdpSocket::bind(source).unwrap();
    socket
        .set_nonblocking(true)
        .expect("Socket Nonblocking Error");

    const RX_BATCH_SIZE: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);
    let mut buf = [0; 1500];

    loop {
        // Listening for interface
        let recieved = interface.receive(&mut packets);
        if recieved > 0 {
            for i in 0..recieved as usize {
                // send received packet to destination socket
                socket.send_to(packets[i].get_buffer_mut(), destination).expect("Socket send Error");
            }
        }

        // Listening for socket
        match socket.recv_from(&mut buf) {
            Ok(n) => {
                // received data from socket and send to interface
                let mut packet = interface.alloc().unwrap();
                let data = buf.to_vec();
                let mut packets: Vec<pv::Packet> = Vec::with_capacity(RX_BATCH_SIZE as usize);
                packet.replace_data(&data).unwrap();
                packets.push(interface.copy_from(&mut packet).unwrap());
                interface.send(&mut packets);
            },
            Err(e) => panic!("encountered IO error: {e}"),
        }
    }
}
