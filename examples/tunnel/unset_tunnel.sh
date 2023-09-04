#!/bin/bash

sudo ip link delete client_gateway
sudo ip link delete client_tunnel
sudo ip link delete server_gateway
sudo ip netns delete client
sudo ip netns delete server
