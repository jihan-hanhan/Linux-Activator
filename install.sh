#!/usr/bin/env bash
set -euo pipefail

PREFIX=/usr/local/bin

echo "[1/4] 编译…"
cmake -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build --parallel >/dev/null

if command -v activate >/dev/null 2>&1; then
    echo "警告: 检测到系统中已存在 activate 命令: $(command -v activate)"
    read -r -p "继续安装将使其被遮蔽, 是否继续? [y/N] " ans
    [[ "$ans" =~ ^[Yy]$ ]] || { echo "已取消。"; exit 1; }
fi

echo "[2/4] 安装到 $PREFIX …"
sudo install -m 0755 build/activate         "$PREFIX/activate"
sudo install -m 0755 build/activate-daemon  "$PREFIX/activate-daemon"

if ! ldconfig -p 2>/dev/null | grep -q libQt6Widgets; then
    echo "警告: 未检测到 Qt6 Widgets 运行时, 请先安装 qt6-base。"
fi

echo "[3/4] 初始化配置文件…"
"$PREFIX/activate" --init

echo "[4/4] 配置登录自启…"
mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/activate-linux.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Activation Service
Comment=Desktop Activation Service
Exec=$PREFIX/activate-daemon
Terminal=false
NoDisplay=true
X-GNOME-Autostart-enabled=true
EOF

mkdir -p "$HOME/.config/systemd/user"
cat > "$HOME/.config/systemd/user/activate-linux.service" <<EOF
[Unit]
Description=Desktop Activation Service
After=graphical-session.target

[Service]
ExecStart=$PREFIX/activate-daemon
Restart=on-failure

[Install]
WantedBy=default.target
EOF
systemctl --user daemon-reload 2>/dev/null || true
systemctl --user enable --now activate-linux.service 2>/dev/null || true

echo "完成。"
