use packetvisor::pv;
use signal_hook::SigId;
use std::{
    env,
    io::Error,
    sync::atomic::{AtomicBool, Ordering},
    sync::Arc,
};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        println!("./ipv6 <interface name>");
        std::process::exit(-1);
    }

    let mut if_name = String::new();
    let chunk_size: u32 = 2048;
    let chunk_count: u32 = 1024;
    let rx_ring_size: u32 = 64;
    let tx_ring_size: u32 = 64;
    let filling_ring_size: u32 = 64;
    let completion_ring_size: u32 = 64;

    for i in 1..args.len() {
        match i {
            1 => {
                if_name = args[1].clone();
            }
            _ => {
                panic!("abnormal index");
            }
        }
    }

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

    /* execute pv_open() */
    let pv_open_option: Option<pv::Nic> = pv::pv_open(
        &if_name,
        chunk_size,
        chunk_count,
        rx_ring_size,
        tx_ring_size,
        filling_ring_size,
        completion_ring_size,
    );
    let mut nic: pv::Nic;
    if let Some(a) = pv_open_option {
        nic = a;
    } else {
        panic!("nic allocation failed!");
    }

    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size: u32 = 64;
    let mut packets: Vec<pv::Packet> = Vec::with_capacity(rx_batch_size as usize);

    while !term.load(Ordering::Relaxed) {
        let received: u32 = pv::pv_receive(&mut nic, &mut packets, rx_batch_size);

        if received > 0 {
            for i in (0..received).rev() {
                unsafe {
                    let payload_ptr = packets[i as usize]
                        .buffer
                        .add(packets[i as usize].start as usize)
                        .cast_const();
                    if std::ptr::read(payload_ptr.offset(12)) == 0x86 // Ethertype == 0x86DD (IPv6)
                        && std::ptr::read(payload_ptr.offset(13)) == 0xDD
                        && std::ptr::read(payload_ptr.offset(14)) >> 4 == 6 // IP version == 6
                    {
                        packet_dump(&mut packets[i as usize]);
                    }
                }
                pv::pv_free(&mut nic, &mut packets, i as usize);
            }
        }
    }

    pv::pv_close(nic);
    println!("PV END");
}

fn packet_dump(packet: &pv::Packet) {
    let buffer_address: *const u8 = packet.buffer.cast_const();

    let length: u32 = packet.end - packet.start;
    let mut count: u32 = 0;

    unsafe {
        loop {
            let read_offset: usize = count as usize;
            let read_address: *const u8 = buffer_address.add(read_offset);
            print!("{:02X?} ", std::ptr::read(read_address));

            count += 1;
            if count == length {
                break;
            } else if count % 8 == 0 {
                print!(" ");
                if count % 16 == 0 {
                    println!();
                }
            }
        }
    }
    println!("\n-------\n");
}
