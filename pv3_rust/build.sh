#!/bin/bash

export PATH="$PATH:/home/$SUDO_USER/.cargo/bin"

echo "Build packetvisor"
rustup default stable
cargo build --release

echo "Apply capabilities to run as non-root"
cp target/release/pv3_rust pv3_rust

setcap CAP_SYS_ADMIN,CAP_NET_ADMIN,CAP_NET_RAW,CAP_DAC_OVERRIDE+ep pv3_rust
getcap pv3_rust
