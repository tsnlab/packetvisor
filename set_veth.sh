#!/bin/sh

# 가상 인터페이스 (veth0, veth1), (veth2, veth3)을 host에 생성
sudo ip link add veth0 type veth peer name veth1
sudo ip link add veth2 type veth peer name veth3

# 임의 namespace 생성 및 lo(loopback interface) 활성
sudo ip netns add test1
sudo ip netns exec test1 ip link set dev lo up
sudo ip netns add test2
sudo ip netns exec test2 ip link set dev lo up

# veth1, veth3을 임의 생성한 test1, test2로 옮기기 (host -> test_namespace)
sudo ip link set veth1 netns test1
sudo ip link set veth3 netns test2

# 가상 인터페이스에 IPv4, IPv6 주소 할당
sudo ip addr add 10.0.0.4/24 dev veth0
sudo ip addr add 2001:db8::1/64 dev veth0
sudo ip netns exec test1 ip addr add 10.0.0.5/24 dev veth1
sudo ip netns exec test1 ip addr add 2001:db8::2/64 dev veth1

sudo ip addr add 10.0.0.6/24 dev veth2
sudo ip netns exec test2 ip addr add 10.0.0.7/24 dev veth3

# 가상 인터페이스 활성화
sudo ip link set dev veth0 up
sudo ip netns exec test1 ip link set dev veth1 up
sudo ip link set dev veth2 up
sudo ip netns exec test2 ip link set dev veth3 up

# ethtool 설치
sudo apt install ethtool -y

# offload 기능 비활성화
sudo ip netns exec test1 ethtool --offload veth1 tx off
sudo ip netns exec test1 ethtool --offload veth1 rx off
sudo ip netns exec test2 ethtool --offload veth3 tx off
sudo ip netns exec test2 ethtool --offload veth3 rx off

# 가상 인터페이스 확인
echo "interface status in host"
ifconfig
echo "interface status in test1"
sudo ip netns exec test1 ifconfig
echo "interface status in test2"
sudo ip netns exec test2 ifconfig
