#!/bin/sh

# --------------------------------------------------
# Wi-Fi 停止脚本
# 功能：断开当前 Wi-Fi 连接，释放 IP，关闭接口，关闭电源
# 用法: /development/wifi/wifi_stop.sh
# 返回: SUCCESS 或 ERROR 消息（供 CGI 解析）
# --------------------------------------------------

export PATH="/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

CONF_FILE="/tmp/wpa_supplicant_dynamic.conf"
LOG_FILE="/var/log/wifi_connect.log"

log() {
    echo "$(date '+%Y-%m-%d %H:%M:%S') - $*" >> "$LOG_FILE"
}

# === Topeet Wi-Fi 电源控制 (GPIO 148) ===
disable_wifi_power() {
    local gpio=148
    echo "$gpio" > /sys/class/gpio/export 2>/dev/null || true
    echo out > /sys/class/gpio/gpio$gpio/direction 2>/dev/null || true
    echo 0 > /sys/class/gpio/gpio$gpio/value 2>/dev/null || true
    # echo "$gpio" > /sys/class/gpio/unexport 2>/dev/null || true
}

# === 停止指定接口 ===
stop_interface() {
    local iface=$1
    if ip link show "$iface" >/dev/null 2>&1; then
        log "Stopping Wi-Fi on $iface..."
        
        # 1. 终止相关进程
        pkill -f "wpa_supplicant.*$iface" 2>/dev/null
        pkill -f "dhclient.*$iface" 2>/dev/null
        
        # 2. 释放 DHCP 租约（虽然进程已杀，但尝试释放是个好习惯，如果 dhclient 还在运行的话）
        # dhclient -r "$iface" 2>/dev/null 
        
        # 3. 清除 IP 地址
        ip addr flush dev "$iface" 2>/dev/null
        
        # 4. 关闭接口
        ip link set "$iface" down 2>/dev/null
        
        return 0
    fi
    return 1
}

# 遍历尝试关闭常见 Wi-Fi 接口
STOPPED_ANY=false
for iface in wlan0 p2p0; do
    if stop_interface "$iface"; then
        STOPPED_ANY=true
    fi
done

# 再次确保全局进程已退出（防止残留）
pkill -x wpa_supplicant 2>/dev/null
sleep 1

# 清理临时配置文件
rm -f "$CONF_FILE"

# 关闭 Wi-Fi 模块电源（彻底断电）
disable_wifi_power
log "Wi-Fi power disabled"

if [ "$STOPPED_ANY" = true ]; then
    echo "SUCCESS: Wi-Fi stopped and power disabled"
    exit 0
else
    # 即使没有找到活跃接口，我们也执行了清理和断电，通常也算成功
    echo "SUCCESS: No active Wi-Fi interface found, but cleanup performed"
    exit 0
fi
