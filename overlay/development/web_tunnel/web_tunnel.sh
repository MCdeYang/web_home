#!/bin/sh

NGROK_TOKEN="38m9o3R3LRYpDL0bnKaxiu3sFEA_53aaPXvh7UxvKp2iJRHBW"
LOCAL_PORT=80
TUNNEL_DIR="/development/web_tunnel"
NGROK_BIN="$TUNNEL_DIR/ngrok"
CONFIG_DIR="$TUNNEL_DIR/ngrok_config"
CONFIG_FILE="$CONFIG_DIR/ngrok.yml"
LOG_FILE="$TUNNEL_DIR/ngrok.log"
PID_FILE="$TUNNEL_DIR/ngrok.pid"
URL_FILE="$TUNNEL_DIR/public_url.txt"
RUNTIME_DIR="$TUNNEL_DIR/tmp"

# =============================
# 创建必要目录
mkdir -p "$CONFIG_DIR" "$RUNTIME_DIR"
chmod 777 "$RUNTIME_DIR" 2>/dev/null

# 初始化配置文件
if [ ! -f "$CONFIG_FILE" ] || ! grep -q "authtoken:" "$CONFIG_FILE" 2>/dev/null; then
    cat > "$CONFIG_FILE" <<EOF
version: "2"
authtoken: $NGROK_TOKEN
web_addr: 127.0.0.1:4040
EOF
    chmod 600 "$CONFIG_FILE"
fi

# 检查依赖
if [ ! -x "$NGROK_BIN" ]; then
    echo "错误: ngrok 二进制文件不存在或不可执行: $NGROK_BIN" >&2
    exit 1
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "错误: curl 未安装，无法获取公网地址" >&2
    exit 1
fi

# ============ stop ============
stop() {
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill "$PID" 2>/dev/null; then
            wait "$PID" 2>/dev/null
            echo "ngrok 已停止 (PID: $PID)"
        fi
        rm -f "$PID_FILE"
    else
        echo "ngrok 未在运行"
    fi
    > "$URL_FILE"
}

# ============ start ===========
start() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "ngrok 已在运行 (PID: $(cat "$PID_FILE"))"
        exit 1
    fi

    echo "启动内网穿透: 本地端口 $LOCAL_PORT → 公网 URL"

    > "$LOG_FILE"
    > "$URL_FILE"
    chmod 666 "$LOG_FILE" 2>/dev/null

    # 启动 ngrok
    env HOME="$RUNTIME_DIR" TMPDIR="$RUNTIME_DIR" \
        "$NGROK_BIN" --config="$CONFIG_FILE" http $LOCAL_PORT \
        --log=stderr \
        --log-level=info \
        > "$LOG_FILE" 2>&1 &
    NGROK_PID=$!
    echo "$NGROK_PID" > "$PID_FILE"

    PUBLIC_URL=""
    i=1
    while [ $i -le 30 ]; do
        if curl -s --connect-timeout 1 http://127.0.0.1:4040 >/dev/null 2>&1; then
            TUNNELS=$(curl -s http://127.0.0.1:4040/api/tunnels)
            PUBLIC_URL=$(echo "$TUNNELS" | grep -o 'https://[^"]*\.ngrok-free\.[^"]*' | head -n1)
            if [ -n "$PUBLIC_URL" ]; then
                break
            fi
        fi
        sleep 1
        i=$((i + 1))
    done

    if [ -n "$PUBLIC_URL" ]; then
        printf '%s' "$PUBLIC_URL" > "$URL_FILE"
        echo "公网地址已保存到: $URL_FILE"
        echo "$PUBLIC_URL"
    else
        echo "未能获取公网地址。检查日志: $LOG_FILE"
        # 输出最后几行日志帮助诊断
        echo "=== 最后10行日志 ==="
        tail -n 10 "$LOG_FILE"
        stop
        exit 1
    fi
}
# ============ restart ==========
restart() {
    stop
    sleep 2
    start
}
# ============ status ===========
status() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat "$PID_FILE")" 2>/dev/null; then
        echo "ngrok 正在运行 (PID: $(cat "$PID_FILE"))"
        if [ -s "$URL_FILE" ]; then
            echo "公网地址: $(cat "$URL_FILE")"
        else
            echo "公网地址尚未就绪"
        fi
    else
        echo "ngrok 未运行"
    fi
}

# ============ 主逻辑 ===========
case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        restart
        ;;
    status)
        status
        ;;
    *)
        echo "用法: $0 {start|stop|restart|status}" >&2
        exit 1
        ;;
esac