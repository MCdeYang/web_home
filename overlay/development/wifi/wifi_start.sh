#!/bin/sh
# 标准化 Wi-Fi 脚本输出，供 CGI 解析

SSID="$1"
PASS="$2"

if [ -z "$SSID" ] || [ -z "$PASS" ]; then
    echo "ERROR: Missing SSID or password" >&2
    exit 1
fi

# 关键：只捕获 nmcli 的标准输出，抑制 sudo 警告（重定向 stderr）
OUTPUT=$(/usr/bin/sudo -n /usr/bin/nmcli device wifi connect "$SSID" password "$PASS" 2>&1)

# 检查 nmcli 是否成功（通过退出码）
if [ $? -eq 0 ]; then
    # 提取连接的接口名（可选）
    # 你可以自定义成功消息
    echo "SUCCESS: Connected to $SSID"
else
    # 输出原始错误（截断避免超长）
    echo "ERROR: ${OUTPUT}" | head -c 200
fi