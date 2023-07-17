#!/bin/sh

# 가상 인터페이스 veth0, veth1를  host에 생성
sudo ip link add veth0 type veth peer name veth1

# 임의 namespace 생성 및 lo(loopback interface) 활성
sudo ip netns add test_namespace
sudo ip netns exec test_namespace ip link set dev lo up

# veth1을 임의 생성한 test_namespace로 옮기기 (host -> test_namespace)
sudo ip link set veth1 netns test_namespace

# 가상 인터페이스 veth0, veth1에 IPv4 주소 할당
sudo ip addr add 2001:db8::1/64 dev veth0
sudo ip netns exec test_namespace ip addr add 2001:db8::2/64 dev veth1

# 가상 인터페이스 veth0, veth1 활성화
sudo ip link set dev veth0 up
sudo ip netns exec test_namespace ip link set dev veth1 up

# 가상 인터페이스 확인
echo "interface status in host"
ifconfig
echo "interface status in test_namespace"
sudo ip netns exec test_namespace ifconfig
