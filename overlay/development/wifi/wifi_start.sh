#!/bin/sh
SSID="$1"
PASS="$2"

if [ -z "$SSID" ]; then
    echo "ERROR: Missing SSID" >&2
    exit 1
fi

# 构造 nmcli 命令
if [ -z "$PASS" ]; then
    # 无密码：开放网络
    CMD="/usr/bin/sudo -n /usr/bin/nmcli device wifi connect '$SSID'"
else
    # 有密码：WPA/WPA2 等
    CMD="/usr/bin/sudo -n /usr/bin/nmcli device wifi connect '$SSID' password '$PASS'"
fi

# 执行命令并捕获输出
OUTPUT=$(eval "$CMD" 2>&1)
RET=$?

if [ $RET -eq 0 ]; then
    echo "SUCCESS: Connected to $SSID"
else
    echo "ERROR: ${OUTPUT}" | head -c 200
fi