#!/bin/bash

echo "update submodules of PV"
git submodule update --init --recursive

echo "Build packetvisor"
cargo build --release

echo "Apply capabilities to run as non-root"
sudo setcap CAP_SYS_ADMIN,CAP_NET_ADMIN,CAP_NET_RAW,CAP_DAC_OVERRIDE+ep target/release/pv3_rust
getcap target/release/pv3_rust

echo "use 'pv3_rust' in /target/release"
