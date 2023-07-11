#!/bin/sh

# 가상 인터페이스 veth0, veth1를  host에 생성
sudo ip link add veth0 type veth peer name veth1
sudo ip link add veth2 type veth peer name veth3

# 임의 namespace 생성 및 lo(loopback interface) 활성
sudo ip netns add test1
sudo ip netns exec test1 ip link set dev lo up
sudo ip netns add test2
sudo ip netns exec test2 ip link set dev lo up

# veth1을 임의 생성한 test_namespace로 옮기기 (host -> test_namespace)
sudo ip link set veth1 netns test1
sudo ip link set veth3 netns test2

# 가상 인터페이스 veth0, veth1에 IPv4 주소 할당
sudo ip addr add 10.0.0.4/24 dev veth0
sudo ip netns exec test1 ip addr add 10.0.0.5/24 dev veth1
sudo ip addr add 10.0.0.6/24 dev veth2
sudo ip netns exec test2 ip addr add 10.0.0.7/24 dev veth3

# 가상 인터페이스 veth0, veth1 활성화
sudo ip link set dev veth0 up
sudo ip netns exec test1 ip link set dev veth1 up
sudo ip link set dev veth2 up
sudo ip netns exec test2 ip link set dev veth3 up

# 가상 인터페이스 확인
echo "interface status in host"
ifconfig
# echo "interface status in test_namespace"
# sudo ip netns exec test_namespace ifconfig
