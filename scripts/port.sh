#!/bin/sh

print_eth_port_status() {
    local port=$1
    
    # One `swconfig dev switch0 show` wastes more time than 4 `port show`
    if [ -n "`ip link show | grep $port@eth0 | grep 'state UP'`" ]; then
        echo 1
    else
        echo 0
    fi
}

print_usb_port_status() {
    if [ "`ls -1 /sys/bus/usb/devices | wc -l`" -gt 8 ]; then
        echo 1
    else
        echo 0
    fi
}

print_eth_port_status lan2 # lan2@eth0 is LAN1 on label
print_eth_port_status lan1 # lan1@eth0 is LAN2 on label
print_eth_port_status lan3 # lan3@eth0 is LAN3 on label
print_eth_port_status wan # WAN
print_usb_port_status