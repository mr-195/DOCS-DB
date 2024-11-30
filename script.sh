#!/bin/bash

ip link add veth0 type veth peer name veth1
ip link set veth0 netns NS1
ip link set veth1 netns NS2


ip link add veth2 type veth peer name veth3
ip link set veth2 netns NS2
ip link set veth3 netns NS3



ip netns exec NS1 ip link set veth0 up

ip netns exec NS2 ip link set veth1 up
ip netns exec NS2 ip link set veth2 up

ip netns exec NS3 ip link set veth3 up


ip netns exec NS1 ip addr add 10.0.0.1/24 dev veth0


ip netns exec NS2 ip addr add 10.0.0.2/24 dev veth1
ip netns exec NS2 ip addr add 10.0.1.1/24 dev veth2


ip netns exec NS3 ip addr add 10.0.1.2/24 dev veth3


ip netns exec NS1 ip route add 10.0.1.0/24 via 10.0.0.2


ip netns exec NS3 ip route add 10.0.0.0/24 via 10.0.1.1


ip netns exec NS1 ip addr show
ip netns exec NS1 ip route show


ip netns exec NS2 ip addr show
ip netns exec NS2 ip route show


ip netns exec NS3 ip addr show
ip netns exec NS3 ip route show

