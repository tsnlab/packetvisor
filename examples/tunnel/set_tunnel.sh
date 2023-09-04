#!/bin/sh

# Create namespace
sudo ip netns add client
sudo ip netns add server

# Create veth
sudo ip link add client_gateway type veth peer name client
sudo ip link add server_gateway type veth peer name server



# Move veth to namespace
sudo ip link set client netns client
sudo ip link set server netns server


# Assign IP address
sudo ip netns exec client ip addr add 192.168.0.42/24 dev client
sudo ip addr add 192.168.0.41/24 dev client_gateway
sudo ip addr add 192.168.0.43/24 dev server_gateway
sudo ip netns exec server ip addr add 192.168.0.44/24 dev server


# Activate veth
sudo ip netns exec client ip link set dev client up
sudo ip link set dev client_gateway up
sudo ip link set dev server_gateway up
sudo ip netns exec server ip link set dev server up

# Inactivate offload
sudo ip netns exec client ethtool --offload client tx off
sudo ip netns exec client ethtool --offload client rx off
sudo ip netns exec server ethtool --offload server tx off
sudo ip netns exec server ethtool --offload server rx off
