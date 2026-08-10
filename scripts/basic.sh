#!/bin/sh
. /etc/os-release
. /etc/openwrt_release

PRODUCT_NAME_FULL=$(cat /etc/board.json | jsonfilter -e "@.model.name")
PRODUCT_NAME=${PRODUCT_NAME_FULL#* } # Remove first word to save space

WAN_IFNAME=$(uci get network.wan.device)
MAC_ADDR=$(ifconfig $WAN_IFNAME | grep -oE "([0-9A-Z]{2}:){5}[0-9A-Z]{2}")

CPU_TEMP=$(($(cat /sys/class/thermal/thermal_zone0/temp) / 1000))

HW_VERSION="A1"
#${LEDE_DEVICE_REVISION:0:2}
FW_VERSION=${DISTRIB_REVISION:0:17}

echo $PRODUCT_NAME

if [ $(uci get k3screenctrl.@general[0].showmore) -eq 1 ]; then
    echo U:$CPU_TEMP *C
    used=`free | grep Mem | awk '{print$3}'`
    all=`free | grep Mem | awk '{print$2}'`
    LOAD=`uptime | awk -F "average:" '{print$2}' | awk -F "," '{print$1}'`
    _up=$(cut -d. -f1 /proc/uptime)
    _d=$((_up/86400)); _h=$(((_up%86400)/3600)); _m=$(((_up%3600)/60))
    if [ $_d -gt 0 ]; then UPTIME=$(printf "UP %dd %d:%02d" $_d $_h $_m); else UPTIME=$(printf "UP %d:%02d" $_h $_m); fi
    echo U:$LOAD R:$((100*$used/$all))%
    echo $UPTIME
    echo $DISTRIB_DESCRIPTION
else
    echo $HW_VERSION
    echo $FW_VERSION
    echo $FW_VERSION
    echo $MAC_ADDR
fi
