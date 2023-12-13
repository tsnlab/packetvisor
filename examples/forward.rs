use clap::{arg, value_parser, Command};

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
        .arg(arg!(dump: -d --dump "Dump packets").required(false))
        .get_matches();

    let name1 = matches.get_one::<String>("nic1").unwrap().clone();
    let name2 = matches.get_one::<String>("nic2").unwrap().clone();
    let chunk_size = *matches.get_one::<usize>("chunk_size").unwrap();
    let chunk_count = *matches.get_one::<usize>("chunk_count").unwrap();
    let rx_ring_size = *matches.get_one::<usize>("rx_ring_size").unwrap();
    let tx_ring_size = *matches.get_one::<usize>("tx_ring_size").unwrap();
    let filling_ring_size = *matches.get_one::<usize>("fill_ring_size").unwrap();
    let completion_ring_size = *matches.get_one::<usize>("completion_ring_size").unwrap();
    let dump = *matches.get_one::<bool>("dump").unwrap_or(&false);

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

    let umem_rc = pv::Pool::new(
        chunk_size,
        chunk_count,
        filling_ring_size,
        completion_ring_size,
        true,
    )
    .expect("Failed to create umem");

    let mut nic1 = match pv::Nic::new(&name1, &umem_rc, tx_ring_size, rx_ring_size) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC1: {}", err);
        }
    };

    let mut nic2 = match pv::Nic::new(&name2, &umem_rc, tx_ring_size, rx_ring_size) {
        Ok(nic) => nic,
        Err(err) => {
            panic!("Failed to create NIC2: {}", err);
        }
    };

    while !term.load(Ordering::Relaxed) {
        let processed2 = forward(&mut nic2, &mut nic1, dump);
        let processed1 = forward(&mut nic1, &mut nic2, dump);

        if processed1 + processed2 == 0 {
            thread::sleep(Duration::from_millis(100));
        } else if dump {
            println!("Processed {}, {} packets", processed1, processed2)
        }
    }
}

fn forward(nic1: &mut pv::Nic, nic2: &mut pv::Nic, dump: bool) -> usize {
    /* initialize rx_batch_size and packet metadata */
    let rx_batch_size = 64;
    let mut packets = nic1.receive(rx_batch_size);
    let count = packets.len();

    if count == 0 {
        return 0;
    }

    if dump {
        for packet in packets.iter_mut() {
            packet.dump();
        }
    }

    for retry in (0..3).rev() {
        match (nic2.send(&mut packets), retry) {
            (cnt, _) if cnt > 0 => break, // Success
            (0, 0) => {
                // Failed 3 times
                println!("Failed to send packets");
                break;
            }
            _ => continue, // Retrying
        }
    }

    count
}
