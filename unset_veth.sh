#!/bin/sh

# 생성한 veth, namespace 제거
sudo ip link delete veth0
sudo ip netns delete test_namespace
