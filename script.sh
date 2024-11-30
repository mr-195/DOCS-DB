#!/bin/bash

# Ensure the script is run with root privileges
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root"
    exit
fi

# Clean up previous namespaces (if any)
ip netns del NS1 2>/dev/null
ip netns del NS2 2>/dev/null
ip netns del NS3 2>/dev/null

# Create network namespaces
ip netns add NS1
ip netns add NS2
ip netns add NS3

# Create veth pairs
ip link add veth0 type veth peer name veth1
ip link add veth2 type veth peer name veth3

# Assign veth interfaces to namespaces
ip link set veth0 netns NS1
ip link set veth1 netns NS2
ip link set veth2 netns NS2
ip link set veth3 netns NS3

# Bring up loopback interfaces in all namespaces
ip netns exec NS1 ip link set lo up
ip netns exec NS2 ip link set lo up
ip netns exec NS3 ip link set lo up

# Set up interfaces and IP addresses in NS1
ip netns exec NS1 ip addr add 10.0.0.1/24 dev veth0
ip netns exec NS1 ip link set dev veth0 up

# Set up interfaces in NS2 and create the bridge
ip netns exec NS2 ip link set dev veth1 up
ip netns exec NS2 ip link set dev veth2 up

# Install bridge-utils if not installed
if ! command -v brctl &> /dev/null; then
    echo "bridge-utils not found, installing..."
    apt-get update
    apt-get install -y bridge-utils
fi

ip netns exec NS2 brctl addbr br0
ip netns exec NS2 brctl addif br0 veth1
ip netns exec NS2 brctl addif br0 veth2
ip netns exec NS2 ip link set dev br0 up

# Set up interfaces and IP addresses in NS3
ip netns exec NS3 ip addr add 10.0.0.3/24 dev veth3
ip netns exec NS3 ip link set dev veth3 up

# Introduce delay and packet loss on veth3 in NS3
ip netns exec NS3 tc qdisc add dev veth3 root netem delay 100ms loss 5%

echo "Namespace setup complete."

# Test connectivity
echo "Testing connectivity from NS1 to NS3:"
ip netns exec NS1 ping -c 4 10.0.0.3
