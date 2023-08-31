#!/bin/sh
clear
echo "\n--------client--------"
sudo ip netns exec client ip -br a
echo "--------host--------"
sudo ip -br a
echo "--------server--------"
sudo ip netns exec server ip -br a
