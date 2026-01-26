#!/bin/bash

# ====== 配置区（按需修改）======
NGROK_TOKEN="你的_ngrok_authtoken"        # 必填！去 https://dashboard.ngrok.com/get-started/your-authtoken 获取
LOCAL_PORT=8000                          # 你的 Web 服务监听的端口（如 80, 3000, 8080 等）
NGROK_BIN="./ngrok"
LOG_FILE="ngrok.log"
PID_FILE="ngrok.pid"
# =============================

if [ "$NGROK_TOKEN" = "你的_ngrok_authtoken" ]; then
    echo "❌ 请先编辑脚本，填入你的 NGROK_TOKEN！"
    echo "👉 注册地址: https://ngrok.com"
    exit 1
fi

# 检查是否已安装 ngrok，若无则自动下载 aarch64 版本
if [ ! -f "$NGROK_BIN" ]; then
    echo "📦 正在下载 ngrok for aarch64..."
    wget -q https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-arm64.tgz -O ngrok.tgz
    tar -xzf ngrok.tgz ngrok
    chmod +x ngrok
    rm -f ngrok.tgz
    echo "✅ ngrok 已安装"
fi

# 配置 authtoken（仅首次需要）
if ! grep -q "authtoken:" ~/.ngrok2/ngrok.yml 2>/dev/null; then
    echo "🔑 配置 ngrok authtoken..."
    ./ngrok config add-authtoken "$NGROK_TOKEN" >/dev/null 2>&1
fi

start() {
    if [ -f "$PID_FILE" ] && kill -0 "$(cat $PID_FILE)" 2>/dev/null; then
        echo "⚠️  ngrok 已在运行 (PID: $(cat $PID_FILE))"
        exit 1
    fi

    echo "🚀 启动内网穿透: 本地端口  $LOCAL_PORT → 公网 URL"
    nohup ./ngrok http " $LOCAL_PORT" > " $LOG_FILE" 2>&1 &
    echo $! > " $PID_FILE"
    sleep 3

    # 尝试从日志中提取公网 URL
    PUBLIC_URL= $(grep -o 'https://[a-zA-Z0-9\-]*\.ngrok-free\.app' "$LOG_FILE" | head -1)
    if [ -n "$PUBLIC_URL" ]; then
        echo "🌐 你的公网访问地址是:"
        echo "   $PUBLIC_URL"
        echo ""
        echo "📌 提示: 地址会随每次重启变化，演示时直接分享此链接即可！"
    else
        echo "⏳ 正在启动... 请稍等几秒后查看日志:"
        echo "   tail -f $LOG_FILE"
    fi
}

stop() {
    if [ -f "$PID_FILE" ]; then
        PID= $(cat "$PID_FILE")
        kill "$PID" 2>/dev/null
        rm -f "$PID_FILE"
        echo "⏹️  ngrok 已停止 (PID: $PID)"
    else
        echo "⚠️  ngrok 未在运行"
    fi
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 2
        start
        ;;
    *)
        echo "用法: $0 {start|stop|restart}"
        echo "示例: $0 start"
        exit 1
        ;;
esac
