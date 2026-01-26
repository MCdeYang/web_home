#!/bin/bash

# 从 RK SDK 同步自定义修改到 mygit 仓库
# 用途：在 SDK 中改完 overlay/mk-rootfs.sh 后，运行此脚本保存到 Git

set -e

MYGIT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SDK_UBUNTU_DIR="$HOME/workspace/code/rk/rk356x_linux/ubuntu"

echo "从 SDK 同步最新修改到 mygit..."
echo "源目录: $SDK_UBUNTU_DIR"
echo "目标目录: $MYGIT_DIR"

#同步 overlay 目录
rsync -av --delete "$SDK_UBUNTU_DIR/overlay/" "$MYGIT_DIR/overlay/"

#同步 mk-rootfs.sh
if [ -f "$SDK_UBUNTU_DIR/mk-rootfs.sh" ]; then
    cp "$SDK_UBUNTU_DIR/mk-rootfs.sh" "$MYGIT_DIR/"
    echo "已更新 mk-rootfs.sh"
fi

echo "同步完成！现在可以 git add / commit / push"
