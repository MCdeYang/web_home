#!/bin/bash -e

chroot_dir=binary

# 默认版本
if [ -z "$VERSION" ]; then
    VERSION="ubuntu20"
fi

echo -e "\033[36m Building rootfs for $VERSION\033[0m"

# === 1. 清理旧目录（先卸载所有挂载点）===
if [ -d "$chroot_dir" ]; then
    echo "Unmounting existing mounts..."
    sudo umount -l "$chroot_dir"/{dev/pts,dev,sys,proc} 2>/dev/null || true
    sudo rm -rf "$chroot_dir"
    echo "Deleted old binary directory."
fi

# === 解压基础系统镜像 ===
echo -e "\033[36m Extracting base image...\033[0m"
case $VERSION in
    "ubuntu20")
        if [ ! -f ubuntu-focal-arm64.tar.xz ]; then
            echo "ERROR: ubuntu-focal-arm64.tar.xz not found!"
            exit 1
        fi
        sudo tar -xf ubuntu-focal-arm64.tar.xz
        ;;
    "ubuntu22")
        if [ ! -f ubuntu-jammy-arm64.tar.xz ]; then
            echo "ERROR: ubuntu-jammy-arm64.tar.xz not found!"
            exit 1
        fi
        sudo tar -xf ubuntu-jammy-arm64.tar.xz
        ;;
    *)
        echo "ERROR: Unsupported VERSION='$VERSION'. Use 'ubuntu20' or 'ubuntu22'."
        exit 1
        ;;
esac
# === 下载 ngrok 到 overlay/development/web_tunnel/ ===
NGROK_DIR="overlay/development/web_tunnel"
LOCAL_TGZ="ngrok-v3-stable-linux-arm64.tgz"

mkdir -p "$NGROK_DIR"
if [ -f "$NGROK_DIR/ngrok" ]; then
    echo "Removing existing ngrok binary..."
    rm -f "$NGROK_DIR/ngrok"
fi
if [ ! -f "$NGROK_DIR/ngrok" ]; then
    echo "Checking for local ngrok archive..."
    if [ -f "$LOCAL_TGZ" ]; then
        echo "Using local $LOCAL_TGZ"
        cp "$LOCAL_TGZ" /tmp/ngrok.tgz
    else
        echo "Local archive not found, downloading from internet..."
        wget -q --timeout=30 --tries=3 https://bin.equinox.io/c/bNyj1mQVY4c/ngrok-v3-stable-linux-arm64.tgz -O /tmp/ngrok.tgz
        if [ $? -ne 0 ]; then
            echo "ERROR: Failed to download ngrok. Please place '$LOCAL_TGZ' in current directory and retry."
            exit 1
        fi
    fi
    tar -xzf /tmp/ngrok.tgz -C "$NGROK_DIR" ngrok
    chmod +x "$NGROK_DIR/ngrok"
    rm -f /tmp/ngrok.tgz
    echo "ngrok placed in overlay/development/web_tunnel/"
fi
# === 应用 overlay ===
if [ -d overlay ]; then
    echo "Applying overlay..."
    sudo cp -rfp overlay/* "${chroot_dir}/" || true
fi
# === 允许 nobody 无密码执行 nmcli Wi-Fi 连接 ===
echo "Configuring sudoers for 'nobody' Wi-Fi access..."
sudo mkdir -p "${chroot_dir}/etc/sudoers.d"
sudo chown root:root "${chroot_dir}/etc/sudoers.d"    # ←←← 新增这行！
sudo chmod 755 "${chroot_dir}/etc/sudoers.d"         # ←←← 建议加上
cat > /tmp/wifi-nobody <<'EOF'
# Allow user 'nobody' to run nmcli wifi connect without password
nobody ALL=(ALL) NOPASSWD: /usr/bin/nmcli device wifi connect *
EOF
sudo cp /tmp/wifi-nobody "${chroot_dir}/etc/sudoers.d/wifi-nobody"
sudo chmod 440 "${chroot_dir}/etc/sudoers.d/wifi-nobody"
sudo chown root:root "${chroot_dir}/etc/sudoers.d/wifi-nobody"

# === 挂载虚拟文件系统 ===
echo "Mounting virtual filesystems..."
sudo mount -t proc /proc "${chroot_dir}/proc"
sudo mount -t sysfs /sys "${chroot_dir}/sys"
sudo mount -o bind /dev "${chroot_dir}/dev"
sudo mount -o bind /dev/pts "${chroot_dir}/dev/pts"

# === 修复 /dev/null ===
echo "Fixing /dev/null in chroot..."
sudo rm -f "${chroot_dir}/dev/null"
sudo mknod -m 666 "${chroot_dir}/dev/null" c 1 3

# === 在 chroot 中执行 APT 操作 ===
echo "Running apt inside chroot..."
sudo chroot "${chroot_dir}" /bin/bash -c "
    set -e
    export DEBIAN_FRONTEND=noninteractive
        echo 'force-confold' > /etc/dpkg/dpkg.cfg.d/01-no-backup-conf

    # 更新软件源
    apt update

    # 创建 APT 缓存目录（防止 partial 错误）
    mkdir -p /var/cache/apt/archives/partial
    # 只安装你需要的软件（lighttpd）
    # 避免安装 vim（易引发版本冲突）
    apt install -y --no-install-recommends lighttpd
        apt install -y --no-install-recommends libcurl4-openssl-dev
        apt install -y --no-install-recommends libjson-c-dev
        apt install -y --no-install-recommends rsync
        apt install -y --no-install-recommends rsyslog
        #apt install -y --no-install-recommends udhcpc
        
        apt install -y --no-install-recommends ntfs-3g
        #apt install -y --no-install-recommends fcgi
        #apt install -y --no-install-recommends openssl
        #apt install -y --no-install-recommends iproute2
    # apt install -y --no-install-recommends curl wget net-tools

    # 清理缓存
    apt clean
    rm -rf /var/lib/apt/lists/*
"
# === 修复 nobody 的 shell（关键！）===
echo "Fixing nobody shell to /bin/sh for CGI system() calls..."
sudo chroot "${chroot_dir}" /bin/bash -c "
    usermod -s /bin/sh nobody
"
# === 编译 CGI ===
echo "Building CGI programs using Makefile..."
if [ -f overlay/www/cgi-bin/src/Makefile ]; then
    sudo mkdir -p "${chroot_dir}/tmp/build-cgi"
    sudo cp -rf overlay/www/cgi-bin/src/* "${chroot_dir}/tmp/build-cgi/"
    sudo chroot "${chroot_dir}" /bin/bash -c "
        cd /tmp/build-cgi
        make
    "
    sudo rm -rf "${chroot_dir}/tmp/build-cgi"
fi
# === 编译 environment ===
echo "Building environment daemon..."
if [ -f overlay/development/src/main.c ] && \
   [ -f overlay/development/src/environment.c ] && \
   [ -f overlay/development/src/check_task.c ] && \
   [ -f overlay/development/Makefile ]; then
    sudo mkdir -p "${chroot_dir}/tmp/build-daemons"
    sudo cp -r overlay/development/src "${chroot_dir}/tmp/build-daemons/"
    sudo cp -r overlay/development/includes "${chroot_dir}/tmp/build-daemons/"
    sudo cp overlay/development/Makefile "${chroot_dir}/tmp/build-daemons/"
    sudo chroot "${chroot_dir}" /bin/bash -c '
        cd /tmp/build-daemons
        make
    '
    sudo install -m 755 "${chroot_dir}/tmp/build-daemons/environment" "${chroot_dir}/usr/local/bin/"
    sudo rm -rf "${chroot_dir}/tmp/build-daemons"
    echo "environment installed."
else
    echo "Warning: Missing one of:"
    echo "  - overlay/development/src/main.c"
    echo "  - overlay/development/src/environment.c"
    echo "  - overlay/development/src/check_task.c"
    echo "  - overlay/development/Makefile"
    echo "Skipping build."
fi
# === 启用 environment 开机自启 ===
if [ -f "${chroot_dir}/etc/init.d/environment" ]; then
    echo "Enabling environment daemon at boot..."
    sudo chroot "${chroot_dir}" /bin/bash -c "
        chmod +x /etc/init.d/environment
        update-rc.d environment defaults
    "
fi
# === 启用 serial-symlinks 开机自启 ===
if [ -f "${chroot_dir}/etc/init.d/serial-symlinks" ]; then
    echo "Enabling serial-symlinks at boot..."
    sudo chroot "${chroot_dir}" /bin/bash -c "
        chmod +x /etc/init.d/serial-symlinks
        update-rc.d serial-symlinks defaults
    "
fi
# === 安装并启用 create_photos_dir 开机自启 ===
echo "Installing and enabling create_photos_dir init script..."
SRC_INIT="overlay/etc/init.d/S16_create_photos_dir"
DST_NAME="create_photos_dir"  # 去掉 S16_ 前缀
DST_PATH="${chroot_dir}/etc/init.d/${DST_NAME}"
if [ -f "$SRC_INIT" ]; then
    sudo install -m 755 "$SRC_INIT" "$DST_PATH"
    echo "Enabling ${DST_NAME} at boot..."
    sudo chroot "${chroot_dir}" /bin/bash -c "
        update-rc.d ${DST_NAME} defaults
    "
    echo "${DST_NAME} installed and enabled."
else
    echo "Warning: $SRC_INIT not found, skipping."
fi
# === 更改ngrok的属主 ===
echo "Setting ownership for web_tunnel files to nobody..."
if [ -f "${chroot_dir}/development/web_tunnel/ngrok" ]; then
    sudo chown 65534:65534 "${chroot_dir}/development/web_tunnel/ngrok"
    sudo chmod 755 "${chroot_dir}/development/web_tunnel/ngrok"
fi
if [ -f "${chroot_dir}/development/web_tunnel/web_tunnel.sh" ]; then
    sudo chown 65534:65534 "${chroot_dir}/development/web_tunnel/web_tunnel.sh"
    sudo chmod 755 "${chroot_dir}/development/web_tunnel/web_tunnel.sh"
fi
# === 安全卸载所有挂载点 ===
echo "Unmounting virtual filesystems..."
sudo umount -l "${chroot_dir}/dev/pts" 2>/dev/null || true
sudo umount -l "${chroot_dir}/dev"      2>/dev/null || true
sudo umount -l "${chroot_dir}/sys"      2>/dev/null || true
sudo umount -l "${chroot_dir}/proc"     2>/dev/null || true

echo -e "\033[32m RootFS build completed successfully!\033[0m"
