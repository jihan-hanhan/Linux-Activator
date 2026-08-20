# Linux-Activator

一个由配置驱动的 Linux 桌面激活套件。作为一个独立的恶搞/教育项目，它复刻了 Windows 的激活工作流——包括产品密钥对话框、激活状态和水印集成。

二进制文件中不包含硬编码的桌面策略。它们仅作为纯粹的执行器：所有副作用（水印管理、通知、自定义命令）均由用户在 KDL 配置文件中定义。

## 组件

| 组件 | 类型 | 用途 |
|---|---|---|
| `activate` | 可执行文件 (Qt6 Widgets) | CLI 状态查询和图形化激活向导。 |
| `activate-daemon` | 可执行文件 (Qt6 Core) | 后台生命周期执行器，在登录时启动。 |
| `~/activate.kdl` | 配置文件 | 激活状态及生命周期钩子定义（KDL 子集）。 |

## 工作原理

### 状态

激活状态存储在 `~/activate.kdl` 中：

```kdl
activated false
product-key ""
```

这两个键值由程序维护，也可以手动编辑。手动将 `activated` 设置为 `true` 是一种激活系统的有效方法。

### 激活流程

1. 当系统已激活时，`activate` 会打印操作系统版本和激活状态，然后退出。
2. 当未激活时，它会打开一个无边框的激活向导。
3. 语法上有效的密钥由 25 个字母数字字符组成，分为五组（例如 `XXXXX-XXXXX-XXXXX-XXXXX-XXXXX`）。
4. 激活是**模拟**的：每次使用有效密钥尝试激活都有 50% 的成功概率。不会执行任何硬件指纹识别、加密或网络通信。
5. 成功时，状态将写入 `activate.kdl` 并执行 `on-activate` 钩子。失败时，将执行 `on-activation-failed` 钩子。

### 守护进程

`activate-daemon` 是一个单实例进程（锁文件位于 `~/.cache/activate-linux/daemon.lock`），它每 3 秒轮询一次配置，并在相应的生命周期时刻执行钩子块（参见配置参考）。在已激活的机器上，它仅执行一次 `on-boot-activated` 然后退出，不会保持常驻。

## 环境要求

- CMake >= 3.16
- C++17 编译器
- Qt 6 (Core, Gui, Widgets)

## 安装

### 依赖项

Debian/Ubuntu（23.04 或更新版本）：

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libgl1-mesa-dev libxkbcommon-dev
```

Arch Linux / Manjaro：

```bash
sudo pacman -S base-devel cmake qt6-base
```

Fedora：

```bash
sudo dnf install gcc-c++ make cmake qt6-qtbase-devel
```

### 安装脚本（推荐）

```bash
chmod +x install.sh
./install.sh
```

该脚本会编译这两个可执行文件，将它们安装到 `/usr/local/bin`，从默认模板生成 `~/activate.kdl`，并通过 XDG 自动启动条目和 systemd 用户服务注册守护进程以实现登录时自动启动。

### 手动构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo install -m 0755 build/activate        /usr/local/bin/activate
sudo install -m 0755 build/activate-daemon /usr/local/bin/activate-daemon
activate --init    # 生成 ~/activate.kdl
```

## 用法

检查状态（或在未激活时打开向导）：

```bash
activate
```

手动启动守护进程（它通常在登录时启动）：

```bash
activate-daemon
```

## 配置参考 (`~/activate.kdl`)

该文件使用一个小型的 KDL 子集：节点名称、带引号的字符串参数、一级 `{}` 代码块、`//` 注释。程序仅作为执行器；列在代码块中的每个命令都会在相应的时刻按原样运行,以提供较高的可自定义性.

配置文件本身添加了对[activate-linux](https://github.com/MrGlockenspiel/activate-linux)的支持,若您安装了他,则在未激活时右下角会出现"激活Linux"的字样,当激活后就会消失.

您可以在配置文件内调用更多软件,从而更加还原.

### 钩子代码块

| 代码块 | 执行者 | 触发时机 |
|---|---|---|
| `on-boot-unactivated` | 守护进程 | 启动时执行一次，当系统未激活时 |
| `on-unactivated-tick` | 守护进程 | 在未激活状态下每约 3 秒执行一次 |
| `on-boot-activated` | 守护进程 | 启动时执行一次，当系统已激活时（随后守护进程退出） |
| `on-activate` | 守护进程或 UI | 未激活 → 已激活的状态转换时 |
| `on-activation-failed` | UI | 每次激活尝试失败时 |

### 命令动词

| 动词 | 语义 |
|---|---|
| `spawn "prog" "a" …` | 启动分离进程，永不等待 |
| `exec "prog" "a" …` | 启动并等待（最多 5 秒） |
| `sh "line"` | 通过 `/bin/sh -c` 运行，分离执行 |
| `sh-wait "line"` | 通过 `/bin/sh -c` 运行，并等待（最多 5 秒） |

默认模板通过带有安全检查的 `sh`/`exec` 行来显示和移除 `activate-linux` 水印；每一行都可以独立编辑或删除。

## 卸载

```bash
systemctl --user disable --now activate-linux.service
rm -f ~/.config/autostart/activate-linux.desktop
rm -f ~/.config/systemd/user/activate-linux.service
sudo rm -f /usr/local/bin/activate /usr/local/bin/activate-daemon
rm -f ~/activate.kdl
rm -rf ~/.cache/activate-linux
```

## 免责声明

本项目是一个独立的恶搞/教育工具。它不隶属于微软公司（Microsoft Corporation）或任何桌面环境供应商，也未经其认可或关联。它不限制对系统的访问，不修改系统文件，并且不施加任何无法通过编辑或删除 `~/activate.kdl` 来移除的限制。

## 鸣谢

感谢Qwen AI (逻辑整理) 与 Trae (代码实现) 的贡献

## 许可证

MIT 许可证

